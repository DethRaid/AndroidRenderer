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
    void add_transition_pass(const TransitionPass& pass);

    /**
     * \brief Adds a pass to copy a buffer into another
     */
    void add_copy_pass(const BufferCopyPass& pass);

    /**
     * \brief Adds a pass to copy mip 0 of one image to mip 0 of the other
     */
    void add_copy_pass(const ImageCopyPass& pass);

    /**
     * Adds a compute pass to the render graph. This lets you do arbitrary work in the execute function of your pass,
     * as opposed to add_compute_dispatch which only dispatches a compute shader
     * 
     * @param pass The compute pass to add to the graph
     */
    void add_pass(ComputePass pass);

    template <typename PushConstantsType = uint32_t>
    void add_compute_dispatch(const ComputeDispatch<PushConstantsType>& dispatch_info);

    template <typename PushConstantsType = uint32_t>
    void add_compute_dispatch(const IndirectComputeDispatch<PushConstantsType>& dispatch_info);

    void add_render_pass(DynamicRenderingPass pass);

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

    void set_resource_usage(const TextureUsageToken& texture_usage_token, bool skip_barrier = true) const;

    /**
     * \brief Retrieves the most recent usage token for the given texture
     */
    TextureUsageToken get_last_usage_token(TextureHandle texture_handle) const;

private:
    RenderBackend& backend;

    ResourceAccessTracker& access_tracker;

    CommandBuffer cmds;

    std::vector<std::function<void()>> post_submit_lambdas;

    uint32_t num_passes = 0;

    void update_accesses_and_issues_barriers(
        const std::vector<TextureUsageToken>& textures,
        const std::vector<BufferUsageToken>& buffers
    ) const;

    void do_compute_shader_copy(const ImageCopyPass& pass);
};

template <typename PushConstantsType>
void RenderGraph::add_compute_dispatch(const ComputeDispatch<PushConstantsType>& dispatch_info) {
    ZoneScoped;

    if(!dispatch_info.name.empty()) {
        cmds.begin_label(dispatch_info.name);
    }

    std::vector<TextureUsageToken> textures;
    textures.reserve(128);

    std::vector<BufferUsageToken> buffers;
    buffers.reserve(128);

    for(const auto& descriptor_set : dispatch_info.descriptor_sets) {
        descriptor_set.get_resource_usage_information(textures, buffers);
    }

    update_accesses_and_issues_barriers(textures, buffers);

    cmds.bind_pipeline(dispatch_info.compute_shader);

    for(auto i = 0u; i < dispatch_info.descriptor_sets.size(); i++) {
        const auto& set = dispatch_info.descriptor_sets.at(i);
        cmds.bind_descriptor_set(i, set);
    }

    auto* push_constants_src = reinterpret_cast<const uint32_t*>(&dispatch_info.push_constants);
    for(auto i = 0u; i < sizeof(PushConstantsType) / sizeof(uint32_t); i++) {
        cmds.set_push_constant(i, push_constants_src[i]);
    }

    cmds.dispatch(dispatch_info.num_workgroups.x, dispatch_info.num_workgroups.y, dispatch_info.num_workgroups.z);

    for (auto i = 0u; i < dispatch_info.descriptor_sets.size(); i++) {
        cmds.clear_descriptor_set(i);
    }

    if(!dispatch_info.name.empty()) {
        cmds.end_label();
    }
}

template <typename PushConstantsType>
void RenderGraph::add_compute_dispatch(const IndirectComputeDispatch<PushConstantsType>& dispatch_info) {
    if(!dispatch_info.name.empty()) {
        cmds.begin_label(dispatch_info.name);
    }

    std::vector<TextureUsageToken> textures;

    std::vector<BufferUsageToken> buffers;

    for(const auto& descriptor_set : dispatch_info.descriptor_sets) {
        descriptor_set.get_resource_usage_information(textures, buffers);
    }

    update_accesses_and_issues_barriers(textures, buffers);

    cmds.bind_pipeline(dispatch_info.compute_shader);

    for(auto i = 0u; i < dispatch_info.descriptor_sets.size(); i++) {
        const auto& set = dispatch_info.descriptor_sets.at(i);
        cmds.bind_descriptor_set(i, set);
    }

    auto* push_constants_src = reinterpret_cast<const uint32_t*>(&dispatch_info.push_constants);
    for(auto i = 0u; i < sizeof(PushConstantsType) / sizeof(uint32_t); i++) {
        cmds.set_push_constant(i, push_constants_src[i]);
    }

    cmds.dispatch_indirect(dispatch_info.dispatch);

    for (auto i = 0u; i < dispatch_info.descriptor_sets.size(); i++) {
        cmds.clear_descriptor_set(i);
    }

    if(!dispatch_info.name.empty()) {
        cmds.end_label();
    }
}
