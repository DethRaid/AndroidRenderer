#pragma once

#include <vector>

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
class RayTracedGlobalIllumination {
public:
    RayTracedGlobalIllumination();

    ~RayTracedGlobalIllumination();

    void trace_global_illumination(
        RenderGraph& graph, const SceneView& view, const RenderScene& scene, TextureHandle gbuffer_normals,
        TextureHandle gbuffer_depth, TextureHandle noise_tex
    );

    /**
     * Retrieves resource usages for applying this effect to the lit scene
     */
    void get_resource_usages(std::vector<TextureUsageToken>& textures, std::vector<BufferUsageToken>& buffers) const;

    /**
     * Applies the RTGI to the scene image with an addative blending pixel shader
     *
     * Optionally can sample rays around the current pixel, decreasing GI noise 
     */
    void add_lighting_to_scene(CommandBuffer& commands, BufferHandle view_buffer, TextureHandle noise_texture) const;

private:
    /**
     * Stores ray start parameters, such as direction and... well, direction
     */
    TextureHandle ray_texture = nullptr;

    /**
     * Per-pixel irradiance, calculated by ray tracing
     */
    TextureHandle ray_irradiance = nullptr;

    RayTracingPipelineHandle rtgi_pipeline = nullptr;

    GraphicsPipelineHandle overlay_pso = nullptr;
};
