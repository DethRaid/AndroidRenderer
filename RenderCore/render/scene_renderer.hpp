#pragma once

#include <imgui.h>

#include "render/bloomer.hpp"
#include "render/backend/render_backend.hpp"
#include "render/scene_view.hpp"
#include "render/material_storage.hpp"
#include "render/texture_loader.hpp"
#include "mesh_storage.hpp"
#include "mip_chain_generator.hpp"
#include "phase/depth_culling_phase.hpp"
#include "render/phase/ui_phase.hpp"
#include "render/phase/lighting_phase.hpp"
#include "render/sdf/lpv_gv_voxelizer.hpp"
#include "sdf/voxel_cache.hpp"
#include "ui/debug_menu.hpp"
#include "visualizers/visualizer_type.hpp"
#include "visualizers/voxel_visualizer.hpp"

class GltfModel;

/**
 * Renders the scene
 */
class SceneRenderer {
public:
    explicit SceneRenderer();

    /**
     * Sets the internal render resolution of the scene, recreating the internal render targets (and framebuffers)
     *
     * @param resolution Resolution to render at
     */
    void set_render_resolution(const glm::uvec2& resolution);

    void set_scene(RenderScene& scene_in);

    /**
     * Do the thing!
     */
    void render();

    SceneTransform& get_local_player();

    TextureLoader& get_texture_loader();

    MaterialStorage& get_material_storage();

    MeshStorage& get_mesh_storage();

    /**
     * Translates the player's location
     */
    void translate_player(const glm::vec3& movement);

    void rotate_player(float delta_pitch, float delta_yaw);

    void set_imgui_commands(ImDrawData* im_draw_data);

    void set_active_visualizer(RenderVisualization visualizer);

private:
    SceneTransform player_view;

    TextureLoader texture_loader;

    MaterialStorage material_storage;

    MeshStorage meshes;

    MipChainGenerator mip_chain_generator;

    Bloomer bloomer;

    RenderScene* scene = nullptr;

    glm::uvec2 scene_render_resolution = glm::uvec2{};

    std::unique_ptr<LightPropagationVolume> lpv;
        
    TextureHandle shadowmap_handle = TextureHandle::None;

    TextureHandle gbuffer_color_handle = TextureHandle::None;

    TextureHandle gbuffer_normals_handle = TextureHandle::None;

    TextureHandle gbuffer_data_handle = TextureHandle::None;

    TextureHandle gbuffer_emission_handle = TextureHandle::None;

    // This should be something like an extracted texture?
    TextureHandle depth_buffer_mip_chain = TextureHandle::None;
    TextureUsageToken last_frame_depth_usage = {};
    
    TextureHandle normal_target_mip_chain = TextureHandle::None;
    TextureUsageToken last_frame_normal_usage = {};

    TextureHandle lit_scene_handle = TextureHandle::None;

    std::vector<TextureHandle> swapchain_images;

    SceneDrawer sun_shadow_drawer;

    DepthCullingPhase depth_culling_phase;

    SceneDrawer depth_prepass_drawer;

    SceneDrawer gbuffer_drawer;
    
    LightingPhase lighting_pass;

    UiPhase ui_phase;

    RenderVisualization active_visualization = RenderVisualization::VoxelizedMeshes;

    VoxelVisualizer voxel_visualizer;

    void create_shadow_render_targets();
    
    void create_scene_render_targets();

    void draw_debug_visualizers(RenderGraph& render_graph);
};
