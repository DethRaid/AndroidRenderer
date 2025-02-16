#pragma once

#include <vector>

#include <glm/mat4x4.hpp>

#include "render/mesh_drawer.hpp"
#include "render/backend/graphics_pipeline.hpp"
#include "render/backend/handles.hpp"
#include "render/backend/compute_shader.hpp"
#include "render/sdf/lpv_gv_voxelizer.hpp"

class ResourceUploadQueue;
struct DescriptorSet;
class VoxelCache;
class RenderGraph;
class RenderBackend;
class ResourceAllocator;
class CommandBuffer;
class RenderScene;
class SceneTransform;
class DirectionalLight;
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
     * Buffer that stores the count of the VPLs in this cascade
     *
     * The buffer is as big as a non-indexed drawcall
     */
    BufferHandle vpl_count_buffer = {};

    /**
     * VPLs in this cascade
     */
    BufferHandle vpl_buffer = {};

    glm::vec3 min_bounds;
    glm::vec3 max_bounds;

    void create_render_targets(ResourceAllocator& allocator);
};

enum class GvBuildMode {
    Off,
    DepthBuffers,
    Voxels,
    PointClouds,
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

    void set_scene_drawer(SceneDrawer&& drawer);

    /**
     * Updates the transform of this LPV to match the scene view
     */
    void update_cascade_transforms(const SceneTransform& view, const DirectionalLight& light);

    void update_buffers(ResourceUploadQueue& queue) const;

    void clear_volume(RenderGraph& render_graph);

    static GvBuildMode get_build_mode();

    void build_geometry_volume_from_voxels(
        RenderGraph& render_graph, const RenderScene& scene
    );

    /**
     * \brief Builds the geometry volume from last frame's depth buffer
     *
     * \param graph Render graph to use
     * \param depth_buffer Scene depth buffer to inject
     * \param normal_target gbuffer normals to inject
     * \param view_uniform_buffer The view uniform buffer that was used to render the depth and normals targets
     * \param resolution Resolution of the depth and normal targets
     */
    void build_geometry_volume_from_scene_view(
        RenderGraph& graph, TextureHandle depth_buffer,
        TextureHandle normal_target, BufferHandle view_uniform_buffer, glm::uvec2 resolution
    ) const;

    static void build_geometry_volume_from_point_clouds(RenderGraph& render_graph, const RenderScene& scene);

    void build_geometry_volume_from_point_clouds(RenderGraph& render_graph, const RenderScene& scene);

    void inject_indirect_sun_light(RenderGraph& graph, RenderScene& scene);

    void dispatch_vpl_injection_pass(
        RenderGraph& graph, uint32_t cascade_index, const CascadeData& cascade
    );

    /**
     * \brief Injects emissive mesh VPL clouds into the LPV
     */
    void inject_emissive_point_clouds(RenderGraph& graph, const RenderScene& scene);

    void propagate_lighting(RenderGraph& render_graph);

    /**
     * Additively renders the LPV onto the bound framebuffer
     *
     * @param commands The command buffer to render with. Should already have a framebuffer bound
     * @param gbuffers_descriptor The descriptor set that contains the gbuffer attachments as input attachments
     * @param scene_view_buffer Buffer with the matrices of the scene view
     */
    void add_lighting_to_scene(
        CommandBuffer& commands, const DescriptorSet& gbuffers_descriptor, BufferHandle scene_view_buffer
    ) const;

    void visualize_vpls(
        RenderGraph& graph, BufferHandle scene_view_buffer, TextureHandle lit_scene, TextureHandle depth_buffer
    );

private:
    RenderBackend& backend;

    // We have a A and B LPV, to allow for ping-ponging during the propagation step

    // Each LPV has a separate texture for the red, green, and blue SH coefficients
    // We store coefficients for the first two SH bands. Future work might add another band, at the cost of 2x the
    // memory

    TextureHandle lpv_a_red = nullptr;
    TextureHandle lpv_a_green = nullptr;
    TextureHandle lpv_a_blue = nullptr;

    TextureHandle lpv_b_red = nullptr;
    TextureHandle lpv_b_green = nullptr;
    TextureHandle lpv_b_blue = nullptr;

    TextureHandle geometry_volume_handle = nullptr;

    VkSampler linear_sampler = VK_NULL_HANDLE;

    ComputePipelineHandle rsm_generate_vpls_pipeline;

    ComputePipelineHandle clear_lpv_shader;

    ComputePipelineHandle inject_voxels_into_gv_shader;

    GraphicsPipelineHandle vpl_injection_pipeline;

    ComputePipelineHandle vpl_injection_compute_pipeline;

    ComputePipelineHandle propagation_shader;

    std::vector<CascadeData> cascades;
    BufferHandle cascade_data_buffer = {};

    /**
     * Renders the LPV into the lighting buffer
     */
    GraphicsPipelineHandle lpv_render_shader;

    /**
     * Renders a visualization of each VPL
     *
     * Takes in a list of VPLs. A geometry shader generates a quad for each, then the fragment shader draws a sphere
     * with the VPL's light on the surface
     */
    GraphicsPipelineHandle vpl_visualization_pipeline;

    SceneDrawer rsm_drawer = {};

    GraphicsPipelineHandle inject_rsm_depth_into_gv_pipeline;
    GraphicsPipelineHandle inject_scene_depth_into_gv_pipeline;

    /**
     * \brief Injects the RSM depth and normals buffers for a given cascade into that cascade's geometry volume
     *
     * This method dispatches one point for each pixel in the depth buffer. The vertex shader reads the depth and
     * normal targets, converts the normals into SH, and dispatches the point to the correct depth layer. The fragment
     * shader simply adds the SH into the cascade target
     *
     * \param graph Render graph to use to perform the work
     * \param cascade Cascade to inject data into
     * \param cascade_index Index of the cascade that we're injecting into
     */
    void inject_rsm_depth_into_cascade_gv(RenderGraph& graph, const CascadeData& cascade, uint32_t cascade_index) const;

    void perform_propagation_step(
        RenderGraph& render_graph,
        TextureHandle read_red, TextureHandle read_green, TextureHandle read_blue,
        TextureHandle write_red, TextureHandle write_green, TextureHandle write_blue,
        bool use_gv
    ) const;
};
