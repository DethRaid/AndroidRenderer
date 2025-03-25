#pragma once

#include <EASTL/vector.h>

#include <tl/optional.hpp>
#include <volk.h>

#include "handles.hpp"
#include "texture_state.hpp"
#include "resource_allocator.hpp"

struct BarrierGroup {
    VkPipelineStageFlags srcStageMask;
    VkPipelineStageFlags dstStageMask;
    VkDependencyFlags dependencyFlags;

    eastl::vector<VkBufferMemoryBarrier> buffer_barriers;

    eastl::vector<VkImageMemoryBarrier> image_barriers;

    eastl::vector<VkMemoryBarrier> memory_barriers;
};

/**
 * A render pass that's been compiled
 */
struct CompiledRenderPass {
    eastl::vector<BarrierGroup> barrier_groups;

    /**
     * VkRenderPass for this renderpass. if empty, this compiled render pass is a subpass. If not empty, this compiled
     * render pass should beg a new render pass
     */
    std::optional<VkRenderPass> render_pass;

    /**
     * Adds a barrier to take the texture_handle from the before state to the after state
     *
     * This issues a very coarse barrier
     *
     * @param texture_handle
     * @param before
     * @param after
     */
    void add_barrier(TextureHandle texture_handle, TextureState before, TextureState after);
};



