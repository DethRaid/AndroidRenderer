#pragma once
#include <glm/vec2.hpp>

#include "render/mip_chain_generator.hpp"
#include "render/backend/compute_shader.hpp"
#include "render/backend/descriptor_set_builder.hpp"
#include "render/backend/graphics_pipeline.hpp"
#include "render/backend/handles.hpp"

class MaterialStorage;
class TextureDescriptorPool;
class RenderGraph;
class ResourceAllocator;
class SceneDrawer;
/**
 * \brief Render phase that culls visible objects and produces a depth buffer in the process
 *
 * This implements a two-pass culling algorithm. First, we draw the objects that were visible last frame. Second, we
 * build a HiZ depth pyramid from the depth buffer. Third, we cull all scene objects against that pyramid. Fourth, we
 * draw objects that were visible this frame but not visible last frame
 *
 * This class is stateful. It owns its depth buffer and the list of visible objects
 */
class DepthCullingPhase {
public:
    explicit DepthCullingPhase();

    ~DepthCullingPhase();

    void set_render_resolution(const glm::uvec2& resolution);

    void render(RenderGraph& graph, const SceneDrawer& drawer, MaterialStorage& materials, BufferHandle view_data_buffer);

    TextureHandle get_depth_buffer() const;

    BufferHandle get_visible_objects() const;

    /**
     * \brief Translates a visibility list to a list of indirect draw commands
     *
     * The visibility list should have a 0 if the primitive at that index is not visible, 1 if it is
     *
     * The returned buffers are destroyed at the beginning of the next frame. Do not cache them
     *
     * \param graph 
     * \param visibility_list 
     * \param primitive_buffer 
     * \param num_primitives 
     * \param mesh_draw_args_buffer 
     * \return A tuple of the draw commands, draw count, and draw ID -> primitive ID mapping buffers
     */
    std::tuple<BufferHandle, BufferHandle, BufferHandle> translate_visibility_list_to_draw_commands(
        RenderGraph& graph, BufferHandle visibility_list, BufferHandle primitive_buffer, uint32_t num_primitives,
        BufferHandle mesh_draw_args_buffer
    ) const;

private:
    TextureHandle depth_buffer = nullptr;

    TextureHandle hi_z_buffer = nullptr;
    VkSampler max_reduction_sampler;

    // Index of the hi-z descriptor in the texture descriptor array
    uint32_t hi_z_index;

    /**
     * \brief uint list of visible primitives
     *
     * 1:1 correspondence with a scene's list of primitives
     *
     * The idea is that each view will have its own DepthCullingPhase and thus this list will be per-view
     */
    BufferHandle visible_objects = {};

    ComputePipelineHandle init_dual_bump_point_pipeline;

    ComputePipelineHandle visibility_list_to_draw_commands;

    MipChainGenerator downsampler;

    ComputePipelineHandle hi_z_culling_shader;

    VkIndirectCommandsLayoutNV command_signature = VK_NULL_HANDLE;

    /**
     * Draws visible objects using device-generated commands
     */
    void draw_visible_objects_dgc(RenderGraph& graph, const SceneDrawer& drawer, MaterialStorage& materials, const DescriptorSet& descriptors, BufferHandle primitive_buffer, uint32_t num_primitives);

    void create_command_signature();

    std::optional<BufferHandle> create_preprocess_buffer(GraphicsPipelineHandle pipeline, uint32_t num_primitives);

    /**
     * Draws visible objects, using a different draw command for each material type
     */
    void draw_visible_objects(
        RenderGraph& graph, const SceneDrawer& drawer, const DescriptorSet& view_descriptor, BufferHandle primitive_buffer,
        uint32_t num_primitives
    ) const;

};
