#include "resource_access_synchronizer.hpp"

#include <algorithm>
#include <magic_enum.hpp>
#include <vulkan/vk_enum_string_helper.h>

#include "core/system_interface.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/utils.hpp"
#include "core/issue_breakpoint.hpp"

static std::shared_ptr<spdlog::logger> logger;

static bool is_write_access(const VkAccessFlagBits2 access) {
    constexpr auto write_mask =
        VK_ACCESS_2_SHADER_WRITE_BIT |
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_2_TRANSFER_WRITE_BIT |
        VK_ACCESS_2_HOST_WRITE_BIT |
        VK_ACCESS_2_MEMORY_WRITE_BIT |
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
        VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR |
        VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR |
        VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT |
        VK_ACCESS_2_COMMAND_PREPROCESS_WRITE_BIT_NV |
        VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR |
        VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_NV |
        VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT |
        VK_ACCESS_2_OPTICAL_FLOW_WRITE_BIT_NV;
    return (access & write_mask) != 0;
}

ResourceAccessTracker::ResourceAccessTracker(RenderBackend& backend_in) : backend{backend_in} {
    if(logger == nullptr) {
        logger = SystemInterface::get().get_logger("ResourceAccessTracker");
        logger->set_level(spdlog::level::debug);
    }
}

void ResourceAccessTracker::set_resource_usage(
    const TextureUsageToken& usage, const bool skip_barrier
) {
    const auto& texture = usage.texture;
    auto aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    if(is_depth_format(texture->create_info.format)) {
        aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    if(std::ranges::find_if(
        initial_texture_usages,
        [=](const TextureUsageToken& token) {
            return token.texture == texture;
        }) == initial_texture_usages.end()) {
        initial_texture_usages.emplace_back(TextureUsageToken{texture, usage.stage, usage.access, usage.layout});

        if(!skip_barrier) {
            logger->trace(
                "Transitioning image {} from {} to {} because it's the first usage of the image",
                texture->name,
                string_VkImageLayout(VK_IMAGE_LAYOUT_UNDEFINED),
                string_VkImageLayout(usage.layout)
            );
            image_barriers.emplace_back(
                VkImageMemoryBarrier2{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                    .dstStageMask = usage.stage,
                    .dstAccessMask = usage.access,
                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = usage.layout,
                    .image = texture->image,
                    .subresourceRange = {
                        .aspectMask = static_cast<VkImageAspectFlags>(aspect),
                        .baseMipLevel = 0,
                        .levelCount = texture->create_info.mipLevels,
                        .baseArrayLayer = 0,
                        .layerCount = texture->create_info.arrayLayers,
                    }
                }
            );
        }
    }

    auto existing_barrier = std::ranges::find_if(
        last_texture_usages,
        [=](const TextureUsageToken& token) {
            return token.texture == texture;
        });

    if(!skip_barrier) {
        if(existing_barrier != last_texture_usages.end()) {
            // Issue a barrier if either (or both) of the accesses require writing
            const auto needs_write_barrier = is_write_access(usage.access) || is_write_access(existing_barrier->access);
            const auto needs_transition_barrier = usage.layout != existing_barrier->layout;
            const auto needs_fussy_shader_barrier = usage.stage != existing_barrier->stage;
            if(needs_write_barrier || needs_transition_barrier || needs_fussy_shader_barrier) {
                if(existing_barrier->layout == VK_IMAGE_LAYOUT_UNDEFINED) {
                    if(texture->name.empty()) {
                        SAH_BREAKPOINT;
                    }
                    logger->trace(
                        "Transitioning image {} from {} to {}",
                        texture->name,
                        magic_enum::enum_name(existing_barrier->layout),
                        magic_enum::enum_name(usage.layout)
                    );
                }
                image_barriers.emplace_back(
                    VkImageMemoryBarrier2{
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                        .srcStageMask = existing_barrier->stage,
                        .srcAccessMask = existing_barrier->access,
                        .dstStageMask = usage.stage,
                        .dstAccessMask = usage.access,
                        .oldLayout = existing_barrier->layout,
                        .newLayout = usage.layout,
                        .image = texture->image,
                        .subresourceRange = {
                            .aspectMask = static_cast<VkImageAspectFlags>(aspect),
                            .baseMipLevel = 0,
                            .levelCount = texture->create_info.mipLevels,
                            .baseArrayLayer = 0,
                            .layerCount = texture->create_info.arrayLayers,
                        }
                    }
                );
            }
        }
    }

    if(existing_barrier != last_texture_usages.end()) {
        *existing_barrier = usage;
    } else {
        last_texture_usages.emplace_back(usage);
    }
}

void ResourceAccessTracker::set_resource_usage(const BufferUsageToken& usage) {
    if(std::ranges::find_if(
        initial_buffer_usages,
        [=](const BufferUsageToken& exiting_use) {
            return exiting_use.buffer == usage.buffer;
        }) == initial_buffer_usages.end()) {
        initial_buffer_usages.emplace_back(usage);
    }

    if(auto itr = std::ranges::find_if(
        last_buffer_usages,
        [=](const BufferUsageToken& last_usage) {
            return last_usage.buffer == usage.buffer;
        }); itr != last_buffer_usages.end()) {
        // Issue a barrier if either (or both) of the accesses require writing
        if(is_write_access(usage.access) || is_write_access(itr->access)) {
            // logger->trace(
            //     "[{}]: Issuing a barrier from access {} to access {}",
            //     buffer->name,
            //     string_VkAccessFlags2(itr->second.access),
            //     string_VkAccessFlags2(access)
            // );

            buffer_barriers.emplace_back(
                VkBufferMemoryBarrier2{
                    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                    .srcStageMask = itr->stage,
                    .srcAccessMask = itr->access,
                    .dstStageMask = usage.stage,
                    .dstAccessMask = usage.access,
                    .buffer = usage.buffer->buffer,
                    .size = usage.buffer->create_info.size,
                }
            );
        }

        *itr = usage;

    } else {
        last_buffer_usages.emplace_back(usage);
    }
}

void ResourceAccessTracker::issue_barriers(const CommandBuffer& commands) {
    const static auto memory_barriers = eastl::vector<VkMemoryBarrier2>{};
    commands.barrier(memory_barriers, buffer_barriers, image_barriers);
    buffer_barriers.clear();
    image_barriers.clear();
}

TextureUsageToken ResourceAccessTracker::get_last_usage_token(const TextureHandle texture_handle) {
    if(auto itr = std::ranges::find_if(
        last_texture_usages,
        [=](const TextureUsageToken& token) {
            return token.texture == texture_handle;
        }); itr != last_texture_usages.end()) {
        return *itr;
    }

    throw std::runtime_error{"Texture has no recent usages!"};
}
