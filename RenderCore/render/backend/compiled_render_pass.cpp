#include "compiled_render_pass.hpp"
#include "utils.hpp"


void CompiledRenderPass::add_barrier(const TextureHandle texture_handle, const TextureState before,
                                     const TextureState after)
{
    const auto& texture = allocator->get_texture(texture_handle);

    // TODO: Provide a way to barrier specific mip levels, for things like texture streaming or making a bloom chain

    auto image_barrier = VkImageMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = to_access_mask(before),
        .dstAccessMask = to_access_mask(after),
        .oldLayout = to_layout(before),
        .newLayout = to_layout(after),
        .image = texture.image,
        .subresourceRange = {
            .aspectMask = static_cast<VkImageAspectFlags>(is_depth_format(texture.create_info.format)
                                                              ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                              : VK_IMAGE_ASPECT_COLOR_BIT),
            .baseMipLevel = 0,
            .levelCount = texture.create_info.mipLevels,
            .baseArrayLayer = 0,
            .layerCount = texture.create_info.arrayLayers
        }
    };

    const auto source_stage = to_stage_flags(before);
    const auto dest_stage = to_stage_flags(after);

    bool in_group = false;

    // Can this barrier fit into an existing barrier group?
    for (auto& barrier_group : barrier_groups)
    {
        if (barrier_group.srcStageMask == source_stage && barrier_group.dstStageMask == dest_stage)
        {
            barrier_group.image_barriers.push_back(image_barrier);
            in_group = true;
            break;
        }
    }

    if (!in_group)
    {
        // Needs its own special group
        // TODO: If we're issuing a barrier for an input attachment, specify the dependency by region. We might be able
        // to better express that with subpass dependencies, though
        barrier_groups.push_back(BarrierGroup{
            .srcStageMask = source_stage,
            .dstStageMask = dest_stage,
            .dependencyFlags = 0,
            .image_barriers = {image_barrier}
        });
    }
}

CompiledRenderPass::CompiledRenderPass(ResourceAllocator& allocator_in) : allocator{&allocator_in}
{
}
