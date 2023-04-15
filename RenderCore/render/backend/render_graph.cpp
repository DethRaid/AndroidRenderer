#include "render_graph.hpp"

#include <magic_enum.hpp>
#include <spdlog/sinks/android_sink.h>
#include <spdlog/logger.h>

#include <volk.h>

#include "utils.hpp"
#include "render/backend/render_backend.hpp"
#include "core/system_interface.hpp"

static std::shared_ptr<spdlog::logger> logger;

bool is_write_access(const VkAccessFlagBits2 access) {
    constexpr auto write_mask =
        VK_ACCESS_2_SHADER_WRITE_BIT |
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_2_TRANSFER_WRITE_BIT |
        VK_ACCESS_2_HOST_WRITE_BIT |
        VK_ACCESS_2_MEMORY_WRITE_BIT |
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
        VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT |
        VK_ACCESS_2_COMMAND_PREPROCESS_WRITE_BIT_NV |
        VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR |
        VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_NV;
    return (access & write_mask) != 0;
}

RenderGraph::RenderGraph(RenderBackend& backend_in) : backend{backend_in},
                                                      cmds{backend.create_graphics_command_buffer()} {
    if (logger == nullptr) {
        logger = SystemInterface::get().get_logger("RenderGraph");
        logger->set_level(spdlog::level::trace);
    }

    cmds.begin();
}

void RenderGraph::add_transition_pass(TransitionPass&& pass) {
    add_compute_pass(
        {
            .name = "Transition pass", .textures = pass.textures, .buffers = pass.buffers,
            .execute = [](CommandBuffer&) {}
        }
    );
}

void RenderGraph::add_copy_pass(ImageCopyPass&& pass) {
    add_compute_pass(
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
            .execute = [=](CommandBuffer& commands) {
                commands.copy_image_to_image(pass.src_image, pass.dst_image);
            }
        }
    );
}

void RenderGraph::add_compute_pass(ComputePass&& pass) {
    if (!pass.name.empty()) {
        logger->debug("Adding compute pass {}", pass.name);

        cmds.begin_label(pass.name);
    }

    for (const auto& buffer_token : pass.buffers) {
        set_resource_usage(buffer_token.first, buffer_token.second.stage, buffer_token.second.access);
    }

    for (const auto& texture_token : pass.textures) {
        set_resource_usage(texture_token.first, texture_token.second);
    }

    issue_barriers(cmds);

    pass.execute(cmds);

    if (!pass.name.empty()) {
        cmds.end_label();
    }
}

void RenderGraph::add_render_pass(RenderPass&& pass) {
    logger->debug("Adding render pass {}", pass.name);

    auto& allocator = backend.get_global_allocator();

    const auto render_pass = allocator.get_render_pass(pass);

    // Update state tracking, accumulating barrier for buffers and non-attachment images
    for (const auto& buffer_token : pass.buffers) {
        set_resource_usage(buffer_token.first, buffer_token.second.stage, buffer_token.second.access);
    }

    for (const auto& texture_token : pass.textures) {
        set_resource_usage(texture_token.first, texture_token.second);
    }

    // Update state tracking for attachment images, and collect attachments for the framebuffer
    auto render_targets = std::vector<TextureHandle>{};
    render_targets.reserve(pass.attachments.size());
    auto depth_target = tl::optional<TextureHandle>{};
    for (const auto& render_target : pass.attachments) {
        const auto& render_target_actual = allocator.get_texture(render_target);
        if (is_depth_format(render_target_actual.create_info.format)) {
            depth_target = render_target;
            set_resource_usage(
                render_target,
                TextureUsageToken{
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                }
            );
        } else {
            render_targets.push_back(render_target);
            set_resource_usage(
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
    auto framebuffer = Framebuffer::create(backend, render_targets, depth_target, render_pass);

    // Begin label, issue and clear barriers, begin render pass proper

    cmds.begin_label(pass.name);

    issue_barriers(cmds);

    cmds.begin_render_pass(render_pass, framebuffer, pass.clear_values);

    auto first_subpass = true;

    for (const auto& subpass : pass.subpasses) {
        if (!first_subpass) {
            cmds.advance_subpass();
        }

        cmds.begin_label(subpass.name);

        subpass.execute(cmds);

        cmds.end_label();

        first_subpass = false;
    }

    cmds.end_render_pass();

    cmds.end_label();

    backend.get_global_allocator().destroy_framebuffer(std::move(framebuffer));
}

void RenderGraph::add_present_pass(PresentPass&& pass) {
    add_transition_pass(
        {
            .textures = {
                {pass.swapchain_image, {VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR}}
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
    add_compute_pass(
        {
            .execute = [label = std::move(label)](const CommandBuffer& commands) {
                commands.begin_label(label);
            }
        }
    );
}

void RenderGraph::end_label() {
    add_compute_pass(
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
    for (const auto& task : post_submit_lambdas) {
        task();
    }

    post_submit_lambdas.clear();
}

void RenderGraph::set_resource_usage(
    const TextureHandle texture, const TextureUsageToken& usage, const bool skip_barrier
) {
    auto& allocator = backend.get_global_allocator();
    const auto& texture_actual = allocator.get_texture(texture);
    auto aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    if (is_depth_format(texture_actual.create_info.format)) {
        aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    if (!initial_texture_usages.contains(texture)) {
        initial_texture_usages.emplace(texture, TextureUsageToken{usage.stage, usage.access, usage.layout});

        if (!skip_barrier) {
            logger->trace(
                "Transitioning image {} from {} to {}", texture_actual.name,
                magic_enum::enum_name(VK_IMAGE_LAYOUT_UNDEFINED), magic_enum::enum_name(usage.layout)
            );
            image_barriers.emplace_back(
                VkImageMemoryBarrier2{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                    .dstStageMask = usage.stage,
                    .dstAccessMask = usage.access,
                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = usage.layout,
                    .image = texture_actual.image,
                    .subresourceRange = {
                        .aspectMask = static_cast<VkImageAspectFlags>(aspect),
                        .baseMipLevel = 0,
                        .levelCount = texture_actual.create_info.mipLevels,
                        .baseArrayLayer = 0,
                        .layerCount = texture_actual.create_info.arrayLayers,
                    }
                }
            );
        }
    }

    if (!skip_barrier) {
        if (const auto& itr = last_texture_usages.find(texture); itr != last_texture_usages.end()) {
            // Issue a barrier if either (or both) of the accesses require writing
            const auto needs_write_barrier = is_write_access(usage.access) || is_write_access(itr->second.access);
            const auto needs_transition_barrier = usage.layout != itr->second.layout;
            const auto needs_fussy_shader_barrier = usage.stage != itr->second.stage;
            if (needs_write_barrier || needs_transition_barrier || needs_fussy_shader_barrier) {
                logger->trace(
                    "Transitioning image {} from {} to {}", texture_actual.name,
                    magic_enum::enum_name(itr->second.layout), magic_enum::enum_name(usage.layout)
                );
                image_barriers.emplace_back(
                    VkImageMemoryBarrier2{
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                        .srcStageMask = itr->second.stage,
                        .srcAccessMask = itr->second.access,
                        .dstStageMask = usage.stage,
                        .dstAccessMask = usage.access,
                        .oldLayout = itr->second.layout,
                        .newLayout = usage.layout,
                        .image = texture_actual.image,
                        .subresourceRange = {
                            .aspectMask = static_cast<VkImageAspectFlags>(aspect),
                            .baseMipLevel = 0,
                            .levelCount = texture_actual.create_info.mipLevels,
                            .baseArrayLayer = 0,
                            .layerCount = texture_actual.create_info.arrayLayers,
                        }
                    }
                );
            }
        }
    }

    last_texture_usages.insert_or_assign(texture, usage);
}

TextureUsageToken RenderGraph::get_last_usage_token(const TextureHandle texture_handle) const {
    if (auto itr = last_texture_usages.find(texture_handle); itr != last_texture_usages.end()) {
        return itr->second;
    }

    throw std::runtime_error{"Texture has no recent usages!"};
}

void RenderGraph::set_resource_usage(
    const BufferHandle buffer, const VkPipelineStageFlags2 pipeline_stage, const VkAccessFlags2 access
) {
    if (!initial_buffer_usages.contains(buffer)) {
        initial_buffer_usages.emplace(buffer, BufferUsageToken{pipeline_stage, access});
    }

    if (const auto& itr = last_buffer_usages.find(buffer); itr != last_buffer_usages.end()) {
        // Issue a barrier if either (or both) of the accesses require writing
        if (is_write_access(access) || is_write_access(itr->second.access)) {
            auto& allocator = backend.get_global_allocator();
            const auto& buffer_actual = allocator.get_buffer(buffer);

            logger->trace(
                "Issuing a barrier from access {:x} to access {:x} for buffer {}", itr->second.access, access,
                buffer_actual.name
            );

            buffer_barriers.emplace_back(
                VkBufferMemoryBarrier2{
                    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                    .srcStageMask = itr->second.stage,
                    .srcAccessMask = itr->second.access,
                    .dstStageMask = pipeline_stage,
                    .dstAccessMask = access,
                    .buffer = buffer_actual.buffer,
                    .size = buffer_actual.create_info.size,
                }
            );
        }
    }

    last_buffer_usages.insert_or_assign(buffer, BufferUsageToken{pipeline_stage, access});
}

void RenderGraph::issue_barriers(const CommandBuffer& cmds) {
    cmds.barrier({}, buffer_barriers, image_barriers);
    buffer_barriers.clear();
    image_barriers.clear();
}
