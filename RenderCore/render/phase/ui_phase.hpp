#pragma once

#include "../backend/handles.hpp"
#include "render/backend/pipeline.hpp"

class SceneTransform;
class SceneRenderer;

/**
 * Upscales the scene render target to the swapchain
 */
class UiPhase {
public:
    explicit UiPhase(SceneRenderer& renderer_in);

    void set_resources(TextureHandle scene_color_in);

    void render(CommandBuffer& commands, SceneTransform& view);
private:
    SceneRenderer& scene_renderer;

    TextureHandle scene_color = TextureHandle::None;

    void create_upscale_pipeline();

    void upscale_scene_color(CommandBuffer& commands);

    Pipeline upsample_pipeline;
};
