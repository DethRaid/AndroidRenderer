#pragma once

#include <glm/vec2.hpp>

#include "render/backend/graphics_pipeline.hpp"
#include "render/backend/handles.hpp"

struct IndirectDrawingBuffers;
class RenderGraph;
class SceneDrawer;

class MotionVectorsPhase {
public:
    MotionVectorsPhase();

    void set_render_resolution(const glm::uvec2& resolution);

    void render(
        RenderGraph& graph, const SceneDrawer& drawer, BufferHandle view_data_buffer, TextureHandle depth_buffer,
        const IndirectDrawingBuffers& buffers
    );

    TextureHandle get_motion_vectors() const;

private:
    GraphicsPipelineHandle motion_vectors_pso = {};

    TextureHandle motion_vectors = nullptr;
};
