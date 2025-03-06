#pragma once
#include "render/backend/graphics_pipeline.hpp"
#include "render/backend/handles.hpp"

class SceneView;
class SceneDrawer;
struct IndirectDrawingBuffers;
class RenderGraph;

class GbuffersPhase {
public:
    GbuffersPhase();

    void render(
        RenderGraph& graph, const SceneDrawer& drawer, const IndirectDrawingBuffers& buffers,
        TextureHandle gbuffer_depth, TextureHandle gbuffer_color, TextureHandle gbuffer_normals,
        TextureHandle gbuffer_data, TextureHandle gbuffer_emission, std::optional<TextureHandle> shading_rate,
        const SceneView& player_view
    );

private:
    GraphicsPipelineHandle opaque_pso;
};
