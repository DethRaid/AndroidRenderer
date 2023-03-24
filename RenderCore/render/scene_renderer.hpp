#pragma once

#include "render/backend/render_backend.hpp"
#include "scene_view.hpp"
#include "render/material_storage.hpp"
#include "render/texture_loader.hpp"
#include "mesh_storage.hpp"
#include "render/phase/ui_phase.hpp"
#include "render/phase/lighting_phase.hpp"
#include "render/sdf/lpv_gv_voxelizer.hpp"

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

    TracyVkCtx get_tracy_context();

    RenderBackend& get_backend();

    SceneTransform& get_local_player();

    TextureLoader& get_texture_loader();

    MaterialStorage& get_material_storage();

    MeshStorage& get_mesh_storage();

    /**
     * Translates the player's location
     */
    void translate_player(const glm::vec3& movement);

private:
    RenderBackend backend;

    SceneTransform player_view;

    TextureLoader texture_loader;

    MaterialStorage materials;

    MeshStorage meshes;

    RenderScene* scene = nullptr;

    glm::uvec2 scene_render_resolution = glm::uvec2{};

    LightPropagationVolume lpv;
        
    TextureHandle shadowmap_handle = TextureHandle::None;

    TextureHandle gbuffer_color_handle = TextureHandle::None;

    TextureHandle gbuffer_normals_handle = TextureHandle::None;

    TextureHandle gbuffer_data_handle = TextureHandle::None;

    TextureHandle gbuffer_emission_handle = TextureHandle::None;
    
    TextureHandle gbuffer_depth_handle = TextureHandle::None;

    TextureHandle last_frame_depth_buffer = TextureHandle::None;

    TextureHandle lit_scene_handle = TextureHandle::None;

    std::vector<TextureHandle> swapchain_images;

    SceneDrawer sun_shadow_drawer;

    SceneDrawer gbuffer_drawer;
    
    LightingPhase lighting_pass;

    UiPhase ui_phase;

    void create_shadow_render_targets();
    
    void create_scene_render_targets();
};
