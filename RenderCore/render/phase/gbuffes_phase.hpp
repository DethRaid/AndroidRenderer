#pragma once

#include <optional>

#include "render/backend/handles.hpp"

class RenderScene;
class SceneView;
struct IndirectDrawingBuffers;
class RenderGraph;

class GbuffersPhase {
public:
    GbuffersPhase();

    void render(
        RenderGraph& graph, const RenderScene& scene, const IndirectDrawingBuffers& buffers,
        TextureHandle gbuffer_depth, TextureHandle gbuffer_color, TextureHandle gbuffer_normals,
        TextureHandle gbuffer_data, TextureHandle gbuffer_emission, std::optional<TextureHandle> shading_rate,
        const SceneView& player_view
    );
};
