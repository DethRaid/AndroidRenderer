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

    void add_present_pass(PresentPass&& pass);

    void begin_label(const std::string& label);

    void end_label();

    void finish() const;

    // Kinda-internal API, useful only to Backend

    /**
     * Removes the command buffer from this RenderGraph
     */
    CommandBuffer&& extract_command_buffer();

    /**
     * Executes all the tasks that should happen after the render graph is executed
     */
    void execute_post_submit_tasks();

    void set_resource_usage(TextureHandle texture, const TextureUsageToken& usage, bool skip_barrier = false);

    /**
     * \brief Retrieves the most recent usage token for the given texture
     */
    TextureUsageToken get_last_usage_token(TextureHandle texture_handle) const;

private:
    RenderBackend& backend;

    CommandBuffer cmds;

    BufferUsageMap initial_buffer_usages;

    BufferUsageMap last_buffer_usages;

    TextureUsageMap initial_texture_usages;

    TextureUsageMap last_texture_usages;

    std::vector<VkBufferMemoryBarrier2> buffer_barriers;

    std::vector<VkImageMemoryBarrier2> image_barriers;

    std::vector<std::function<void()>> post_submit_lambdas;

    void set_resource_usage(BufferHandle buffer, VkPipelineStageFlags2 pipeline_stage, VkAccessFlags2 access);
    
    void issue_barriers(const CommandBuffer& cmds);
};
