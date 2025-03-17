#pragma once

#include <imgui.h>

#include "render/antialiasing_type.hpp"
#include "render/noise_texture.hpp"
#include "render/bloomer.hpp"
#include "render/scene_view.hpp"
#include "render/material_storage.hpp"
#include "render/texture_loader.hpp"
#include "render/mesh_storage.hpp"
#include "render/mip_chain_generator.hpp"
#include "render/procedural_sky.hpp"
#include "render/gi/rtgi.hpp"
#include "render/phase/ambient_occlusion_phase.hpp"
#include "render/phase/depth_culling_phase.hpp"
#include "render/phase/gbuffer_phase.hpp"
#include "render/phase/motion_vectors_phase.hpp"
#include "render/phase/sampling_rate_calculator.hpp"
#include "render/phase/ui_phase.hpp"
#include "render/phase/lighting_phase.hpp"
#include "render/gi/light_propagation_volume.hpp"
#include "ui/debug_menu.hpp"
#include "render/upscaling/upscaler.hpp"
#include "visualizers/visualizer_type.hpp"

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
     * @param new_output_resolution Resolution to render at
     */
    void set_output_resolution(const glm::uvec2& new_output_resolution);

    void set_scene(RenderScene& scene_in);
    /**
     * Do the thing!
     */
    void render();

    SceneView& get_local_player();

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
    SceneView player_view;

    TextureLoader texture_loader;

    MaterialStorage material_storage;

    MeshStorage meshes;

    MipChainGenerator mip_chain_generator;

    Bloomer bloomer;

    RenderScene* scene = nullptr;

    glm::uvec2 output_resolution = {};

    glm::uvec2 scene_render_resolution = glm::uvec2{};

    /**
     * Spatio-temporal blue noise texture, containing 3D vectors in a cosine-weighted hemisphere
     */
    NoiseTexture stbn_3d_unitvec;

    std::unique_ptr<LightPropagationVolume> lpv;

    std::unique_ptr<RayTracedGlobalIllumination> rtgi;

    TextureHandle gbuffer_color_handle = nullptr;

    TextureHandle gbuffer_normals_handle = nullptr;

    TextureHandle gbuffer_data_handle = nullptr;

    TextureHandle gbuffer_emission_handle = nullptr;

    TextureHandle ao_handle = nullptr;

    // This should be something like an extracted texture?
    TextureHandle depth_buffer_mip_chain = nullptr;
    TextureUsageToken last_frame_depth_usage = {};
    
    TextureHandle normal_target_mip_chain = nullptr;
    TextureUsageToken last_frame_normal_usage = {};

    TextureHandle lit_scene_handle = nullptr;

    TextureHandle antialiased_scene_handle = nullptr;

    std::vector<TextureHandle> swapchain_images;

    ProceduralSky sky;

    /**
     * \brief Screen-space camera jitter applied to this frame
     */
    glm::vec2 jitter = {};
    glm::vec2 previous_jitter = {};

    DepthCullingPhase depth_culling_phase;

    MotionVectorsPhase motion_vectors_phase;

    GbufferPhase gbuffer_phase;

    AmbientOcclusionPhase ao_phase;
    
    LightingPhase lighting_pass;

    std::unique_ptr<VRSAA> vrsaa;

    UiPhase ui_phase;

    RenderVisualization active_visualization = RenderVisualization::None;

    std::unique_ptr<IUpscaler> upscaler;

    AntiAliasingType cached_aa = AntiAliasingType::None;

    uint32_t frame_count = 0;

    void set_render_resolution(glm::uvec2 new_render_resolution);

    void create_scene_render_targets();

    void update_jitter();

    void draw_debug_visualizers(RenderGraph& render_graph);

    /**
     * Simple funscreen triangle PSO to copy one sampled image to a render target
     */
    GraphicsPipelineHandle copy_scene_pso;

    VkSampler linear_sampler;

    void evaluate_antialiasing(RenderGraph& render_graph, TextureHandle gbuffer_depth_handle) const;
};
