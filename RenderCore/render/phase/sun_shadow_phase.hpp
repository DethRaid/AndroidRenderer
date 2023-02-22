#pragma once

#include "render/phase/phase_interface.hpp"
#include "render/backend/handles.hpp"

class SceneRenderer;
class RenderScene;
class SunLight;

/**
 * Renders shadows from the sun
 */
class SunShadowPhase {
public:
    explicit SunShadowPhase(SceneRenderer& scene_renderer_in);

    void set_scene(RenderScene& scene_in);

    void render(CommandBuffer& commands, SunLight& light);

private:
    SceneRenderer& scene_renderer;

    RenderScene* scene = nullptr;
};



