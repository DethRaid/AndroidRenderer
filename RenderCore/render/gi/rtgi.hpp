#pragma once

#include <vector>

#include "global_illuminator.hpp"
#include "render/backend/buffer_usage_token.hpp"
#include "render/backend/handles.hpp"
#include "render/backend/texture_usage_token.hpp"

class ProceduralSky;
class RenderScene;
class CommandBuffer;
struct DescriptorSet;
class SceneView;
class RenderGraph;
struct GBuffer;

/**
 * Per-pixel ray traces global illumination, with spatial path reuse
 */
class RayTracedGlobalIllumination : public IGlobalIlluminator {
public:
    static bool should_render();

    RayTracedGlobalIllumination();

    ~RayTracedGlobalIllumination() override;

    void pre_render(
        RenderGraph& graph, const SceneView& view, const RenderScene& scene, TextureHandle noise_tex
    ) override {}

    void post_render(
        RenderGraph& graph, const SceneView& view, const RenderScene& scene, const GBuffer& gbuffer,
        TextureHandle noise_tex
    ) override;

    /**
     * Retrieves resource usages for applying this effect to the lit scene
     */
    void get_lighting_resource_usages(
        std::vector<TextureUsageToken>& textures, std::vector<BufferUsageToken>& buffers
    ) const override;

    /**
     * Applies the RTGI to the scene image with an addative blending pixel shader
     *
     * Optionally can sample rays around the current pixel, decreasing GI noise 
     */
    void render_to_lit_scene(
        CommandBuffer& commands, BufferHandle view_buffer, TextureHandle ao_tex, TextureHandle noise_texture
    ) const override;

private:
    /**
     * Stores ray start parameters, such as direction and... well, direction
     */
    TextureHandle ray_texture = nullptr;

    /**
     * Per-pixel irradiance, calculated by ray tracing
     */
    TextureHandle ray_irradiance = nullptr;

    static inline RayTracingPipelineHandle rtgi_pipeline = nullptr;

    static inline GraphicsPipelineHandle overlay_pso = nullptr;
};
