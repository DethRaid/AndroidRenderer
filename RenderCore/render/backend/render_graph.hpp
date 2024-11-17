#pragma once

#include "render/backend/command_buffer.hpp"
#include "render/backend/render_pass.hpp"

class ResourceAccessTracker;
class RenderBackend;

/**
 * Basic render graph
 *
 * Can automatically handle resource transitions
 *
 * Render passes are always executed in the order they're received. Intended usage is for you to make a new render
 * graph each frame, add passes to it, then submit it to the backend for execution. Passes may not run until the end of
 * the frame, but they'll always run the same frame your submit the graph
 *
 * This render graph does not allocate resources. Resource allocation should be handled with the ResourceAllocator
 * class
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

    /**
     * \brief Adds a pass to copy mip 0 of one image to mip 0 of the other
     */
    void add_copy_pass(ImageCopyPass&& pass);

    [[deprecated("Use add_compute_dispatch")]]
    void add_pass(ComputePass&& pass);

    template <typename PushConstantsType = uint32_t>
    void add_compute_dispatch(const ComputeDispatch<PushConstantsType>& dispatch_info);

    void begin_render_pass(const RenderPassBeginInfo& begin_info);

    void add_subpass(Subpass&& subpass);

    void end_render_pass();

    void add_finish_frame_and_present_pass(const PresentPass& pass);

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

    void set_resource_usage(
        TextureHandle texture_handle, const TextureUsageToken& texture_usage_token, bool skip_barrier = true
    ) const;

    /**
     * \brief Retrieves the most recent usage token for the given texture
     */
    TextureUsageToken get_last_usage_token(TextureHandle texture_handle) const;

private:
    RenderBackend& backend;

    ResourceAccessTracker& access_tracker;

    CommandBuffer cmds;

    std::vector<std::function<void()>> post_submit_lambdas;

    std::optional<RenderPass> current_render_pass;

    void add_render_pass(RenderPass&& pass);

    void update_accesses_and_issues_barriers(
        const std::unordered_map<TextureHandle, TextureUsageToken>& textures,
        const std::unordered_map<BufferHandle, BufferUsageToken>& buffers
    ) const;
};

template <typename PushConstantsType>
void RenderGraph::add_compute_dispatch(const ComputeDispatch<PushConstantsType>& dispatch_info) {
    if (!dispatch_info.name.empty()) {
        cmds.begin_label(dispatch_info.name);
    }

    std::unordered_map<TextureHandle, TextureUsageToken> textures;

    std::unordered_map<BufferHandle, BufferUsageToken> buffers;

    for (const auto& descriptor_set : dispatch_info.descriptor_sets) {
        descriptor_set.get_resource_usage_information(textures, buffers);
    }

    update_accesses_and_issues_barriers(textures, buffers);

    cmds.bind_pipeline(dispatch_info.compute_shader);

    for (auto i = 0u; i < dispatch_info.descriptor_sets.size(); i++) {
        const auto vk_set = dispatch_info.descriptor_sets.at(i).get_vk_descriptor_set();
        cmds.bind_descriptor_set(i, vk_set);
    }

    auto* push_constants_src = reinterpret_cast<const uint32_t*>(&dispatch_info.push_constants);
    for (auto i = 0u; i < sizeof(PushConstantsType) / sizeof(uint32_t); i++) {
        cmds.set_push_constant(i, push_constants_src[i]);
    }

    cmds.dispatch(dispatch_info.num_workgroups.x, dispatch_info.num_workgroups.y, dispatch_info.num_workgroups.z);

    if (!dispatch_info.name.empty()) {
        cmds.end_label();
    }
}
