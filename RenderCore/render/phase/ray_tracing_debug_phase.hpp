#pragma once
#include <cstdint>

#include "render/backend/handles.hpp"

struct GBuffer;
class RenderScene;
class RenderGraph;
class SceneView;

class RayTracingDebugPhase {
public:
    static uint32_t get_debug_mode();

    void raytrace(
        RenderGraph& graph, const SceneView& view, const RenderScene& scene, const GBuffer& gbuffer,
        TextureHandle output_texture
    );

private:
    RayTracingPipelineHandle pipeline = nullptr;
};
