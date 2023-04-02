#pragma once

#include "render/backend/command_buffer.hpp"
#include "render/backend/render_pass.hpp"

class RenderBackend;

/**
 * Basic render graph
 *
 * Can automatically handle resource transitions
 *
 * Render passes are always executed in the order they're received. Intended usage is for you to make a new render
 * graph each frame, add passes to it, then submit it to the backend for execution
 *
 * This render graph does not allocate resources. Resource allocation should be handled with the ResourceAllocator
 * class
 *
 * This render graph does not create render passes or subpasses. Individual passes are free to use render passes or
 * subpasses as they with, within the limits of what Vulkan allows
 */
class RenderGraph {
public:
    explicit RenderGraph(RenderBackend& backend_in);

    /**
     * Adds a pass that inserts a barrier for access to some resources
     *
     * Ex: Add a barrier pass for the primitive data buffer after you upload data to it. Multiple future passes use
     * the primitive data buffer, describing access for it on ever pass that uses it would be cumbersome
     */
    void add_transition_pass(TransitionPass&& pass);

    void add_compute_pass(ComputePass&& pass);

    void add_render_pass(RenderPass&& pass);

    void begin_label(const std::string& label);

    void end_label();

    void finish();

private:
    RenderBackend& backend;

    CommandBuffer cmds;

    BufferUsageMap initial_buffer_usages;

    BufferUsageMap last_buffer_usages;

    TextureUsageMap initial_texture_usages;

    TextureUsageMap last_texture_usages;

    std::vector<VkBufferMemoryBarrier2> buffer_barriers;

    std::vector<VkImageMemoryBarrier2> image_barriers;

    void set_resource_usage(BufferHandle buffer, VkPipelineStageFlags2 pipeline_stage, VkAccessFlags2 access);

    void set_resource_usage(
        TextureHandle texture, VkPipelineStageFlags2 pipeline_stage, VkAccessFlags2 access, VkImageLayout layout
    );

    void issue_barriers(const CommandBuffer& cmds);
};
