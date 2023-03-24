#pragma once

#include <vector>

#include <glm/mat4x4.hpp>

#include "mesh_drawer.hpp"
#include "backend/pipeline.hpp"
#include "render/backend/handles.hpp"
#include "render/backend/compute_shader.hpp"
#include "render/sdf/lpv_gv_voxelizer.hpp"

class RenderGraph;
class RenderBackend;
class ResourceAllocator;
class CommandBuffer;
class RenderScene;
class SceneTransform;
class SunLight;
class MeshStorage;

struct CascadeData {
    /**
     * World to cascade matrix. Does not contain a NDC -> UV conversion
     */
    glm::mat4 world_to_cascade;

    /**
     * VP matrix to use when rendering the RSM
     */
    glm::mat4 rsm_vp;

    // Render targets and framebuffer to use
    TextureHandle flux_target;
    TextureHandle normals_target;
    TextureHandle depth_target;

    /**
     * Count of the number of VPLs in this cascade
     */
    BufferHandle count_buffer = BufferHandle::None;

    /**
     * VPLs in this cascade
     */
    BufferHandle vpl_buffer = BufferHandle::None;

    /**
     * 3D Compute shader voxelizer, allegedly useful
     *
     * However, it takes about 7 ms to rasterize Sponza on my RTX 2080 Super. I'm sure there's some things I could do
     * to speed it up - but should I?
     */
    // LpvGvVoxelizer voxels;

    glm::vec3 min_bounds;
    glm::vec3 max_bounds;

    void create_render_targets(ResourceAllocator& allocator);
};

/**
 * A light propagation volume, a la Crytek
 *
 * https://www.advances.realtimerendering.com/s2009/Light_Propagation_Volumes.pdf
 *
 * This is actually cascaded LPVs, but I couldn't find that paper
 *
 * Each cascade is 2x as large as the previous cascade, but has the same number of cells
 */
class LightPropagationVolume {
public:
    explicit LightPropagationVolume(RenderBackend& backend_in);

    void init_resources(ResourceAllocator& allocator);
    
    void set_scene(RenderScene& scene_in, MeshStorage& meshes_in);

    /**
     * Updates the transform of this LPV to match the scene view
     */
    void update_cascade_transforms(const SceneTransform& view, const SunLight& light);

    void update_buffers(CommandBuffer& commands) const;

    void inject_indirect_sun_light(RenderGraph& graph, RenderScene& scene, const MeshStorage& meshes);

    void clear_volume(RenderGraph& render_graph);
    
    void propagate_lighting(RenderGraph& render_graph);

    /**
     * Additively renders the LPV onto the bound framebuffer
     *
     * @param commands The command buffer to render with. Should already have a framebuffer bound
     * @param gbuffers_descriptor The descriptor set that contains the gbuffer attachments as input attachments
     */
    void add_lighting_to_scene(CommandBuffer& commands, VkDescriptorSet gbuffers_descriptor, BufferHandle scene_view_buffer);

private:
    RenderBackend& backend;

    // We have a A and B LPV, to allow for ping-ponging during the propagation step

    // Each LPV has a separate texture for the red, green, and blue SH coefficients
    // We store coefficients for the first two SH bands. Future work might add another band, at the cost of 2x the
    // memory

    TextureHandle lpv_a_red = TextureHandle::None;
    TextureHandle lpv_a_green = TextureHandle::None;
    TextureHandle lpv_a_blue = TextureHandle::None;

    TextureHandle lpv_b_red = TextureHandle::None;
    TextureHandle lpv_b_green = TextureHandle::None;
    TextureHandle lpv_b_blue = TextureHandle::None;

    TextureHandle geometry_volume_handle = TextureHandle::None;

    VkRenderPass rsm_render_pass = VK_NULL_HANDLE;

    VkRenderPass vpl_injection_render_pass = VK_NULL_HANDLE;

    Pipeline vpl_pipeline;

    ComputeShader clear_lpv_shader;

    Pipeline vpl_injection_pipeline;

    ComputeShader propagation_shader;

    std::vector<CascadeData> cascades;
    BufferHandle cascade_data_buffer = BufferHandle::None;

    /**
     * Renders the LPV into the lighting buffer
     */
    Pipeline lpv_render_shader;

    SceneDrawer rsm_drawer;

    void perform_propagation_step(RenderGraph& render_graph,
                                  TextureHandle read_red, TextureHandle read_green, TextureHandle read_blue,
                                  TextureHandle write_red, TextureHandle write_green, TextureHandle write_blue) const;

};



