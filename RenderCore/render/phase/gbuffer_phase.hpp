#pragma once

#include <optional>

#include "render/backend/handles.hpp"

class RenderScene;
class SceneView;
struct IndirectDrawingBuffers;
class RenderGraph;

class GbufferPhase {
public:
    GbufferPhase();

    void render(
        RenderGraph& graph, const RenderScene& scene, const IndirectDrawingBuffers& buffers,
        const IndirectDrawingBuffers& visible_masked_buffers, TextureHandle gbuffer_depth, TextureHandle gbuffer_color,
        TextureHandle gbuffer_normals, TextureHandle gbuffer_data, TextureHandle gbuffer_emission,
        std::optional<TextureHandle> shading_rate, const SceneView& player_view
    );
};
