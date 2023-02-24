#pragma once

#include "render/backend/render_backend.hpp"
#include "scene_view.hpp"
#include "render/material_storage.hpp"
#include "render/texture_loader.hpp"
#include "render/backend/framebuffer.hpp"
#include "render/phase/gbuffer_phase.hpp"
#include "mesh_storage.hpp"
#include "render/phase/ui_phase.hpp"
#include "render/phase/lighting_phase.hpp"
#include "render/phase/sun_shadow_phase.hpp"
#include "render/phase/rsm_vpl_pass.hpp"

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

    SceneView& get_local_player();

    TextureLoader& get_texture_loader();

    MaterialStorage& get_material_storage();

    MeshStorage& get_mesh_storage();

private:
    RenderBackend backend;

    SceneView player_view;

    TextureLoader texture_loader;

    MaterialStorage materials;

    MeshStorage meshes;

    RenderScene* scene = nullptr;

    glm::uvec2 scene_render_resolution = glm::uvec2{};

    LightPropagationVolume lpv;

    VkRenderPass shadow_render_pass = VK_NULL_HANDLE;

    VkRenderPass scene_render_pass = VK_NULL_HANDLE;

    VkRenderPass ui_render_pass = VK_NULL_HANDLE;

    TextureHandle shadowmap_handle = TextureHandle::None;

    TextureHandle gbuffer_color_handle = TextureHandle::None;

    TextureHandle gbuffer_normals_handle = TextureHandle::None;

    TextureHandle gbuffer_data_handle = TextureHandle::None;

    TextureHandle gbuffer_emission_handle = TextureHandle::None;
    
    TextureHandle gbuffer_depth_handle = TextureHandle::None;

    TextureHandle lit_scene_handle = TextureHandle::None;

    std::vector<TextureHandle> swapchain_images;

    SunShadowPhase sun_shadow_pass;

    GbufferPhase gbuffer_pass;

    LightingPhase lighting_pass;

    UiPhase ui_phase;

    void create_shadow_render_targets();

    void create_render_passes();

    void create_scene_render_targets_and_framebuffers();
};
