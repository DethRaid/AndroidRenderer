#pragma once

#include "phase_interface.hpp"

class CommandBuffer;
class SceneRenderer;
class RenderScene;

/**
 * Rendering phase that renders the gbuffer
 */
class GbufferPhase : public PhaseInterface {
public:
    explicit GbufferPhase(SceneRenderer& scene_renderer_in);

    void set_scene(RenderScene& scene_in);

    void render(CommandBuffer& commands, SceneView& view) override;

private:
    SceneRenderer& scene_renderer;

    RenderScene* scene = nullptr;
};



