#include "render_graph.hpp"

#include <magic_enum.hpp>
#include <spdlog/sinks/android_sink.h>
#include <spdlog/logger.h>

#include "pipeline_cache.hpp"
#include "render/backend/resource_access_synchronizer.hpp"
#include "render/backend/utils.hpp"
#include "render/backend/render_backend.hpp"
#include "core/system_interface.hpp"

static std::shared_ptr<spdlog::logger> logger;

RenderGraph::RenderGraph(RenderBackend& backend_in) : backend{backend_in},
                                                      access_tracker{backend.get_resource_access_tracker()},
                                                      cmds{
                                                          backend.create_graphics_command_buffer(
                                                              "Render graph command buffer"
                                                          )
                                                      } {
    if(logger == nullptr) {
        logger = SystemInterface::get().get_logger("RenderGraph");
        logger->set_level(spdlog::level::info);
    }

    cmds.begin();
}

void RenderGraph::add_transition_pass(const TransitionPass& pass) {
    add_pass(
        {
            .name = "transition_pass", .textures = pass.textures, .buffers = pass.buffers,
            .execute = [](CommandBuffer&) {}
        }
    );
}

void RenderGraph::add_copy_pass(const BufferCopyPass& pass) {
    add_pass(
        {
            .name = pass.name,
            .buffers = {
                {
                    pass.src, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT
                },
                {
                    pass.dst, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT
                }
            },
            .execute = [=](const CommandBuffer& commands) {
                commands.copy_buffer_to_buffer(pass.dst, 0, pass.src, 0);
            }
        }
    );
}

void RenderGraph::add_copy_pass(const ImageCopyPass& pass) {
    if(is_depth_format(pass.dst->create_info.format) || is_depth_format(pass.src->create_info.format)) {
        do_compute_shader_copy(pass);

    } else {
        add_pass(
            {
                .name = pass.name,
                .textures = {
                    {
                        pass.src, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                    },
                    {
                        pass.dst, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                    }
                },
                .execute = [=](const CommandBuffer& commands) {
                    commands.copy_image_to_image(pass.src, pass.dst);
                }
            }
        );
    }
}

void RenderGraph::add_pass(ComputePass pass) {
    num_passes++;
    if(!pass.name.empty()) {
        logger->trace("Adding compute pass {}", pass.name);

        cmds.begin_label(pass.name);
    }

    for (const auto& set : pass.descriptor_sets) {
        set.get_resource_usage_information(pass.textures, pass.buffers);
    }

    update_accesses_and_issues_barriers(pass.textures, pass.buffers);

    access_tracker.issue_barriers(cmds);

    {
        ZoneTransientN(zone, pass.name.c_str(), true);
        TracyVkZoneTransient(cmds.get_tracy_context(), vk_zone, cmds.get_vk_commands(), pass.name.c_str(), true)

        pass.execute(cmds);
    }

    if(!pass.name.empty()) {
        cmds.end_label();
    }
}

void RenderGraph::add_render_pass(DynamicRenderingPass pass) {
    num_passes++;

    logger->trace("Adding dynamic render pass {}", pass.name);

    for(const auto& set : pass.descriptor_sets) {
        set.get_resource_usage_information(pass.textures, pass.buffers);
    }

    update_accesses_and_issues_barriers(pass.textures, pass.buffers);

    auto num_layers = 0u;
    for(const auto& attachment_token : pass.color_attachments) {
        access_tracker.set_resource_usage(
            TextureUsageToken{
                .texture = attachment_token.image,
                .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .access = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
            });

        // Assumes that all render targets have the same depth
        num_layers = attachment_token.image->create_info.extent.depth;
    }

    if(pass.depth_attachment) {
        access_tracker.set_resource_usage(
            TextureUsageToken{
                .texture = pass.depth_attachment->image,
                .stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                .access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
            });

        // Assumes that all render targets have the same depth
        num_layers = pass.depth_attachment->image->create_info.extent.depth;
    }

    if(pass.shading_rate_image) {
        access_tracker.set_resource_usage(
            {
                .texture = *pass.shading_rate_image,
                .stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,
                .access = VK_ACCESS_2_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR,
                .layout = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR
            }
        );
    }

    cmds.begin_label(pass.name);
    {
        TracyVkZoneTransient(backend.get_tracy_context(), tracy_zone, cmds.get_vk_commands(), pass.name.c_str(), true)

        access_tracker.issue_barriers(cmds);

        auto render_area_size = glm::uvec2{};
        if(pass.depth_attachment) {
            render_area_size = {
                pass.depth_attachment->image->create_info.extent.width,
                pass.depth_attachment->image->create_info.extent.height
            };
        } else if(!pass.color_attachments.empty()) {
            render_area_size = {
                pass.color_attachments[0].image->create_info.extent.width,
                pass.color_attachments[0].image->create_info.extent.height
            };
        }

        auto rendering_info = RenderingInfo{
            .render_area_begin = {},
            .render_area_size = render_area_size,
            .layer_count = num_layers,
            .view_mask = pass.view_mask.value_or(0),
            .color_attachments = pass.color_attachments,
            .depth_attachment = pass.depth_attachment,
            .shading_rate_image = pass.shading_rate_image,
        };

        cmds.begin_rendering(rendering_info);

        pass.execute(cmds);

        cmds.end_rendering();
    }
    cmds.end_label();
}

void RenderGraph::update_accesses_and_issues_barriers(
    const std::vector<TextureUsageToken>& textures,
    const std::vector<BufferUsageToken>& buffers
) const {
    for(const auto& buffer_token : buffers) {
        access_tracker.set_resource_usage(buffer_token);
    }

    for(const auto& texture_token : textures) {
        access_tracker.set_resource_usage(texture_token);
    }

    access_tracker.issue_barriers(cmds);
}

static ComputePipelineHandle image_copy_shader = nullptr;

void RenderGraph::do_compute_shader_copy(const ImageCopyPass& pass) {
    if(pass.dst->create_info.extent.width != pass.src->create_info.extent.width ||
        pass.dst->create_info.extent.height != pass.src->create_info.extent.height) {
        throw std::runtime_error{"Source and dest images have different extents, cannot copy!"};
    }

    if(!image_copy_shader) {
        image_copy_shader = RenderBackend::get().get_pipeline_cache().create_pipeline("shaders/util/image_copy.comp.spv");
    }

    const auto set = RenderBackend::get().get_transient_descriptor_allocator()
                                         .build_set(image_copy_shader, 0)
                                         .bind(pass.src)
                                         .bind(pass.dst)
                                         .build();

    const auto resolution = glm::uvec2{pass.dst->create_info.extent.width, pass.dst->create_info.extent.height};
    add_compute_dispatch<glm::uvec2>(
        {
            .name = "Image copy",
            .descriptor_sets = {set},
            .push_constants = resolution,
            .num_workgroups = {(resolution.x + 7) / 8, (resolution.y + 7) / 8, 1},
            .compute_shader = image_copy_shader,
        });
}

void RenderGraph::add_finish_frame_and_present_pass(const PresentPass& pass) {
    add_transition_pass(
        {
            .textures = {
                {
                    pass.swapchain_image, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_NONE,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                }
            },
        }
    );

    post_submit_lambdas.emplace_back(
        [this]() {
            backend.flush_batched_command_buffers();
            backend.present();
        }
    );
}

void RenderGraph::begin_label(const std::string& label) {
    add_pass(
        {
            .execute = [label = std::move(label)](const CommandBuffer& commands) {
                commands.begin_label(label);
            }
        }
    );
}

void RenderGraph::end_label() {
    add_pass(
        {
            .execute = [](const CommandBuffer& commands) {
                commands.end_label();
            }
        }
    );
}

void RenderGraph::finish() const {
    cmds.end();
}

CommandBuffer&& RenderGraph::extract_command_buffer() {
    return std::move(cmds);
}

void RenderGraph::execute_post_submit_tasks() {
    for(const auto& task : post_submit_lambdas) {
        task();
    }

    post_submit_lambdas.clear();

    logger->debug("Executed {} passes", num_passes);

    num_passes = 0;
}

void RenderGraph::set_resource_usage(const TextureUsageToken& texture_usage_token, const bool skip_barrier) const {
    access_tracker.set_resource_usage(texture_usage_token, skip_barrier);
}

TextureUsageToken RenderGraph::get_last_usage_token(const TextureHandle texture_handle) const {
    return access_tracker.get_last_usage_token(texture_handle);
}
