#pragma once

#include "render/backend/handles.hpp"

struct DescriptorSet;
class SceneView;
class RenderGraph;
/**
 * Per-pixel ray traces global illumination, with spatial path reuse
 */
class RayTracedGlobalIllumination {
public:
    RayTracedGlobalIllumination();

    ~RayTracedGlobalIllumination();

    void trace_global_illumination(
        RenderGraph& graph, const SceneView& view, TextureHandle gbuffer_normals, TextureHandle gbuffer_data,
        TextureHandle gbuffer_depth
    );

    /**
     * Applies the RTGI to the scene image with an addative blending pixel shader
     *
     * Optionally can sample rays around the current pixel, decreasing GI noise 
     */
    void render(RenderGraph& graph, const SceneView& view, const DescriptorSet& gbuffers_set);

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
};
