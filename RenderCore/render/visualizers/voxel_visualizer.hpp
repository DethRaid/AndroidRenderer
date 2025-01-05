#pragma once

#include "render/backend/graphics_pipeline.hpp"
#include "render/backend/handles.hpp"

class RenderScene;
class RenderGraph;

class VoxelVisualizer {
public:
    explicit VoxelVisualizer();

    void render(RenderGraph& render_graph, const RenderScene& scene, TextureHandle output_image, BufferHandle view_uniform_buffer) const;

private:
    GraphicsPipelineHandle visualization_pipeline;

    BufferHandle cube_index_buffer;
    BufferHandle cube_vertex_buffer;
};

