#pragma once

#include <glm/vec2.hpp>

#include "render/backend/handles.hpp"

class RenderGraph;
class SceneDrawer;

class MotionVectorsPhase
{
public:
    MotionVectorsPhase();

    void set_render_resolution(const glm::uvec2& resolution);

    void render(RenderGraph& graph, const SceneDrawer& drawer, BufferHandle view_data_buffer, BufferHandle visible_objects_buffer);

    TextureHandle get_motion_vectors() const;

private:
    TextureHandle motion_vectors = nullptr;
};

