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

RenderGraph::RenderGraph(RenderBackend& backend_in) : backend{backend_in}, cmds{backend.create_command_buffer()} {
    if (logger == nullptr) {
        logger = SystemInterface::get().get_logger("RenderGraph");
        logger->set_level(spdlog::level::info);
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

void RenderGraph::add_compute_pass(ComputePass&& pass) {
    if (!pass.name.empty()) {
        logger->debug("Adding compute pass {}", pass.name);

        cmds.begin_label(pass.name);
    }

    for (const auto& buffer_token : pass.buffers) {
        set_resource_usage(buffer_token.first, buffer_token.second.stage, buffer_token.second.access);
    }

    for (const auto& texture_token : pass.textures) {
        set_resource_usage(
            texture_token.first, texture_token.second.stage, texture_token.second.access, texture_token.second.layout
        );
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
        set_resource_usage(
            texture_token.first, texture_token.second.stage, texture_token.second.access, texture_token.second.layout
        );
    }

    // Update state tracking for attachment images, and collect attachments for the framebuffer
    auto render_targets = std::vector<TextureHandle>{};
    render_targets.reserve(pass.render_targets.size());
    auto depth_target = tl::optional<TextureHandle>{};
    for (const auto& render_target : pass.render_targets) {
        const auto& render_target_actual = allocator.get_texture(render_target);
        if (is_depth_format(render_target_actual.create_info.format)) {
            depth_target = render_target;
            set_resource_usage(
                render_target,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            );
        } else {
            render_targets.push_back(render_target);
            set_resource_usage(
                render_target,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
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

void RenderGraph::begin_label(const std::string& label) {
    add_compute_pass({
        .execute = [label = std::move(label)](CommandBuffer& commands) {
            commands.begin_label(label);
        }
    });
}

void RenderGraph::end_label() {
    add_compute_pass({
        .execute = [](CommandBuffer& commands) {
            commands.end_label();
        }
        });
}

void RenderGraph::finish() {
    cmds.end();

    backend.submit_command_buffer(std::move(cmds));
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

void RenderGraph::set_resource_usage(
    const TextureHandle texture, const VkPipelineStageFlags2 pipeline_stage, const VkAccessFlags2 access,
    const VkImageLayout layout
) {
    auto& allocator = backend.get_global_allocator();
    const auto& texture_actual = allocator.get_texture(texture);
    auto aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    if (is_depth_format(texture_actual.create_info.format)) {
        aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    if (!initial_texture_usages.contains(texture)) {
        initial_texture_usages.emplace(texture, TextureUsageToken{pipeline_stage, access, layout});

        logger->trace(
            "Transitioning image {} from {} to {}", texture_actual.name,
            magic_enum::enum_name(VK_IMAGE_LAYOUT_UNDEFINED), magic_enum::enum_name(layout)
        );
        image_barriers.emplace_back(
            VkImageMemoryBarrier2{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                .dstStageMask = pipeline_stage,
                .dstAccessMask = access,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = layout,
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

    if (const auto& itr = last_texture_usages.find(texture); itr != last_texture_usages.end()) {
        // Issue a barrier if either (or both) of the accesses require writing
        if (is_write_access(access) || is_write_access(itr->second.access)) {
            logger->trace(
                "Transitioning image {} from {} to {}", texture_actual.name,
                magic_enum::enum_name(itr->second.layout), magic_enum::enum_name(layout)
            );
            image_barriers.emplace_back(
                VkImageMemoryBarrier2{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = itr->second.stage,
                    .srcAccessMask = itr->second.access,
                    .dstStageMask = pipeline_stage,
                    .dstAccessMask = access,
                    .oldLayout = itr->second.layout,
                    .newLayout = layout,
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

    last_texture_usages.insert_or_assign(texture, TextureUsageToken{pipeline_stage, access, layout});
}

void RenderGraph::issue_barriers(const CommandBuffer& cmds) {
    cmds.barrier({}, buffer_barriers, image_barriers);
    buffer_barriers.clear();
    image_barriers.clear();
}
