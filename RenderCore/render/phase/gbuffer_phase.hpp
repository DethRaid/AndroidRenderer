#pragma once

#include <optional>

#include "render/backend/handles.hpp"

struct GBuffer;
class RenderScene;
class SceneView;
struct IndirectDrawingBuffers;
class RenderGraph;

class GbufferPhase {
public:
    GbufferPhase();

    void render(
        RenderGraph& graph, const RenderScene& scene, const IndirectDrawingBuffers& buffers,
        const IndirectDrawingBuffers& visible_masked_buffers, const GBuffer& gbuffer,
        std::optional<TextureHandle> shading_rate, const SceneView& player_view
    );
};
