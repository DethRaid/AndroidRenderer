#pragma once

#include "render/backend/handles.hpp"
#include "render/backend/graphics_pipeline.hpp"

class CommandBuffer;
class SceneTransform;
class SceneRenderer;

/**
 * Upscales the scene render target to the swapchain
 */
class UiPhase {
public:
    explicit UiPhase(SceneRenderer& renderer_in);

    void set_resources(TextureHandle scene_color_in);

    void render(CommandBuffer& commands, const SceneTransform& view, TextureHandle bloom_texture);
private:
    SceneRenderer& scene_renderer;

    TextureHandle scene_color = TextureHandle::None;

    VkSampler bilinear_sampler;

    void create_upscale_pipeline();

    void upscale_scene_color(CommandBuffer& commands, TextureHandle bloom_texture);

    GraphicsPipelineHandle upsample_pipeline;
};
