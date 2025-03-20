#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "render/backend/handles.hpp"
#include "backend/graphics_pipeline.hpp"
#include "shared/sun_light_constants.hpp"

struct NoiseTexture;
struct GBuffer;
class ResourceUploadQueue;
class RenderScene;
class RenderGraph;
struct DescriptorSet;
class ResourceAllocator;

class CommandBuffer;
class SceneView;
class RenderBackend;

enum class SunShadowMode {
    Off,
    CascadedShadowMaps,
    RayTracing
};

/**
 * Represents a directional light
 */
class DirectionalLight {
public:
    static SunShadowMode get_shadow_mode();

    explicit DirectionalLight();

    void update_shadow_cascades(const SceneView& view);

    void set_direction(const glm::vec3& direction);

    void set_color(glm::vec4 color);

    void update_buffer(ResourceUploadQueue& queue);

    BufferHandle get_constant_buffer() const;

    GraphicsPipelineHandle& get_pipeline();

    glm::vec3 get_direction() const;

    /**
     * Rasterizes the cascaded shadow maps for this light
     */
    void render_shadows(RenderGraph& graph, const RenderScene& scene) const;

    /**
     * Renders this light's contribution to the scene, using a fullscreen triangle and additive blending
     */
    void render(CommandBuffer& commands, const SceneView& view) const;

    /**
     * Renders this light's contribution to the scene, using ray tracing to compute shadows
     */
    void raytrace(
        RenderGraph& graph, const SceneView& view, const GBuffer& gbuffer, const RenderScene& scene,
        TextureHandle lit_scene, const NoiseTexture& noise
    );

    TextureHandle get_shadowmap_handle() const;

private:
    bool sun_buffer_dirty = true;

    SunLightConstants constants = {};

    BufferHandle sun_buffer = {};

    std::vector<glm::mat4> world_to_ndc_matrices;
    BufferHandle world_to_ndc_matrices_buffer = nullptr;

    GraphicsPipelineHandle pipeline = {};

    TextureHandle shadowmap_handle = nullptr;

    RayTracingPipelineHandle rt_pipeline = nullptr;

    uint32_t frame_index = 0;
};
