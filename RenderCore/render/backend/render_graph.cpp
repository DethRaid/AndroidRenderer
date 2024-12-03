#include "render_graph.hpp"

#include <magic_enum.hpp>
#include <spdlog/sinks/android_sink.h>
#include <spdlog/logger.h>

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

void RenderGraph::add_transition_pass(TransitionPass&& pass) {
    add_pass(
        {
            .name = "Transition pass", .textures = pass.textures, .buffers = pass.buffers,
            .execute = [](CommandBuffer&) {}
        }
    );
}

void RenderGraph::add_copy_pass(ImageCopyPass&& pass) {
    add_pass(
        {
            .name = pass.name,
            .textures = {
                {
                    pass.src_image,
                    {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL}
                },
                {
                    pass.dst_image,
                    {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL}
                }
            },
            .execute = [=](const CommandBuffer& commands) {
                commands.copy_image_to_image(pass.src_image, pass.dst_image);
            }
        }
    );
}

void RenderGraph::add_pass(ComputePass&& pass) {
    if(!pass.name.empty()) {
        logger->debug("Adding compute pass {}", pass.name);

        cmds.begin_label(pass.name);
    }

    for(const auto& buffer_token : pass.buffers) {
        access_tracker.set_resource_usage(buffer_token.first, buffer_token.second.stage, buffer_token.second.access);
    }

    for(const auto& texture_token : pass.textures) {
        access_tracker.set_resource_usage(texture_token.first, texture_token.second);
    }

    access_tracker.issue_barriers(cmds);

    {
        ZoneTransientN(zone, pass.name.c_str(), true);
        TracyVkZoneTransient(cmds.get_tracy_context(), vk_zone, cmds.get_vk_commands(), pass.name.c_str(), true);

        pass.execute(cmds);
    }

    if(!pass.name.empty()) {
        cmds.end_label();
    }
}

void RenderGraph::begin_render_pass(const RenderPassBeginInfo& begin_info) {
    current_render_pass = RenderPass{
        .name = begin_info.name,
        .textures = begin_info.textures,
        .buffers = begin_info.buffers,
        .descriptor_sets = begin_info.descriptor_sets,
        .attachments = begin_info.attachments,
        .clear_values = begin_info.clear_values,
        .view_mask = begin_info.view_mask
    };
}

void RenderGraph::add_subpass(Subpass&& subpass) {
    current_render_pass->subpasses.emplace_back(std::move(subpass));
}

void RenderGraph::end_render_pass() {
    add_render_pass_internal(std::move(*current_render_pass));
    current_render_pass = std::nullopt;
}

void RenderGraph::add_render_pass(DynamicRenderingPass pass) {
    logger->debug("Adding dynamic render pass {}", pass.name);

    auto& allocator = backend.get_global_allocator();

    for(const auto& set : pass.descriptor_sets) {
        set.get_resource_usage_information(pass.textures, pass.buffers);
    }

    // Update state tracking, accumulating barrier for buffers and non-attachment images
    for(const auto& buffer_token : pass.buffers) {
        access_tracker.set_resource_usage(buffer_token.first, buffer_token.second.stage, buffer_token.second.access);
    }

    for(const auto& texture_token : pass.textures) {
        access_tracker.set_resource_usage(texture_token.first, texture_token.second);
    }

    auto num_layers = 0u;
    for(const auto& attachment_token : pass.color_attachments) {
        access_tracker.set_resource_usage(
            attachment_token.image,
            TextureUsageToken{
                .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .access = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
            });

        // Assumes that all render targets have the same depth
        const auto& attachment_actual = allocator.get_texture(attachment_token.image);
        num_layers = attachment_actual.create_info.extent.depth;
    }

    if(pass.depth_attachment) {
        access_tracker.set_resource_usage(
            pass.depth_attachment->image,
            TextureUsageToken{
                .stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                .access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
            });

        // Assumes that all render targets have the same depth
        const auto& attachment_actual = allocator.get_texture(pass.depth_attachment->image);
        num_layers = std::max(attachment_actual.create_info.extent.depth, attachment_actual.create_info.arrayLayers);
    }


    cmds.begin_label(pass.name);
    {
        TracyVkZoneTransient(backend.get_tracy_context(), tracy_zone, cmds.get_vk_commands(), pass.name.c_str(), true);

        access_tracker.issue_barriers(cmds);

        auto render_area_size = glm::uvec2{};
        if(pass.depth_attachment) {
            const auto& depth_attachment_actual = allocator.get_texture(pass.depth_attachment->image);
            render_area_size = {
                depth_attachment_actual.create_info.extent.width, depth_attachment_actual.create_info.extent.height
            };
        } else if(!pass.color_attachments.empty()) {
            const auto& attachment_actual = allocator.get_texture(pass.color_attachments[0].image);
            render_area_size = {
                attachment_actual.create_info.extent.width, attachment_actual.create_info.extent.height
            };
        }

        auto rendering_info = RenderingInfo{
            .render_area_begin = {},
            .render_area_size = render_area_size,
            .layer_count = num_layers,
            .view_mask = pass.view_mask.value_or(0),
            .color_attachments = pass.color_attachments,
            .depth_attachment = pass.depth_attachment
        };

        cmds.begin_rendering(rendering_info);

        pass.execute(cmds);

        cmds.end_rendering();
    }
    cmds.end_label();
}

void RenderGraph::add_render_pass_internal(RenderPass&& pass) {
    logger->debug("Adding render pass {}", pass.name);

    auto& allocator = backend.get_global_allocator();

    const auto render_pass = allocator.get_render_pass(pass);

    for(const auto& set : pass.descriptor_sets) {
        set.get_resource_usage_information(pass.textures, pass.buffers);
    }

    // Update state tracking, accumulating barrier for buffers and non-attachment images
    for(const auto& buffer_token : pass.buffers) {
        access_tracker.set_resource_usage(buffer_token.first, buffer_token.second.stage, buffer_token.second.access);
    }

    for(const auto& texture_token : pass.textures) {
        access_tracker.set_resource_usage(texture_token.first, texture_token.second);
    }

    // Update state tracking for attachment images, and collect attachments for the framebuffer
    auto render_targets = std::vector<TextureHandle>{};
    render_targets.reserve(pass.attachments.size());
    auto depth_target = std::optional<TextureHandle>{};
    for(const auto& render_target : pass.attachments) {
        const auto& render_target_actual = allocator.get_texture(render_target);
        if(is_depth_format(render_target_actual.create_info.format)) {
            depth_target = render_target;
            access_tracker.set_resource_usage(
                render_target,
                TextureUsageToken{
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                }
            );
        } else {
            render_targets.push_back(render_target);
            access_tracker.set_resource_usage(
                render_target,
                TextureUsageToken{
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                }
            );
        }
    }

    // Create framebuffer
    Framebuffer framebuffer = Framebuffer::create(backend, render_targets, depth_target, render_pass);

    // Begin label, issue and clear barriers, begin render pass proper

    cmds.begin_label(pass.name);

    access_tracker.issue_barriers(cmds);

    cmds.begin_render_pass(render_pass, framebuffer, pass.clear_values);

    auto first_subpass = true;

    for(const auto& subpass : pass.subpasses) {
        if(!first_subpass) {
            cmds.advance_subpass();
        }

        cmds.begin_label(subpass.name);

        subpass.execute(cmds);

        cmds.end_label();

        first_subpass = false;
    }

    cmds.end_render_pass();

    cmds.end_label();


    // If the renderpass has more than one subpass, update the resource access tracker with the final state of all input attachments
    if(pass.subpasses.size() > 1) {
        for(const auto& subpass : pass.subpasses) {
            for(const auto input_attachment_idx : subpass.input_attachments) {
                const auto& input_attachment = pass.attachments[input_attachment_idx];
                const auto& attachment_actual = allocator.get_texture(input_attachment);
                if(is_depth_format(attachment_actual.create_info.format)) {
                    access_tracker.set_resource_usage(
                        input_attachment,
                        {
                            .stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, .access = VK_ACCESS_2_SHADER_READ_BIT,
                            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                        }
                    );
                } else {
                    access_tracker.set_resource_usage(
                        input_attachment,
                        {
                            .stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, .access = VK_ACCESS_2_SHADER_READ_BIT,
                            .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                        }
                    );
                }
            }
        }
    }

    // Don't issue barriers for input attachment. Renderpasses have the necessary barriers between subpasses

    backend.get_global_allocator().destroy_framebuffer(std::move(framebuffer));
}

void RenderGraph::update_accesses_and_issues_barriers(
    const absl::flat_hash_map<TextureHandle, TextureUsageToken>& textures,
    const absl::flat_hash_map<BufferHandle, BufferUsageToken>& buffers
) const {
    for(const auto& buffer_token : buffers) {
        access_tracker.set_resource_usage(buffer_token.first, buffer_token.second.stage, buffer_token.second.access);
    }

    for(const auto& texture_token : textures) {
        access_tracker.set_resource_usage(texture_token.first, texture_token.second);
    }

    access_tracker.issue_barriers(cmds);
}

void RenderGraph::add_finish_frame_and_present_pass(const PresentPass& pass) {
    add_transition_pass(
        {
            .textures = {
                {
                    pass.swapchain_image,
                    {VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR}
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
}

void RenderGraph::set_resource_usage(
    const TextureHandle texture_handle, const TextureUsageToken& texture_usage_token, const bool skip_barrier
) const {
    access_tracker.set_resource_usage(texture_handle, texture_usage_token, skip_barrier);
}

TextureUsageToken RenderGraph::get_last_usage_token(const TextureHandle texture_handle) const {
    return access_tracker.get_last_usage_token(texture_handle);
}
