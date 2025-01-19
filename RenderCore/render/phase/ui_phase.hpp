#pragma once

#include <array>

#include <imgui.h>

#include "render/backend/handles.hpp"
#include "render/backend/graphics_pipeline.hpp"
#include "render/backend/render_graph.hpp"
#include "render/backend/resource_upload_queue.hpp"

class CommandBuffer;
class SceneTransform;
class SceneRenderer;

/**
 * Upscales the scene render target to the swapchain
 */
class UiPhase {
public:
    explicit UiPhase();

    void set_resources(TextureHandle scene_color_in, glm::uvec2 render_resolution_in);

    void add_data_upload_passes(ResourceUploadQueue& queue) const;

    void render(CommandBuffer& commands, const SceneTransform& view, TextureHandle bloom_texture) const;

    void set_imgui_draw_data(ImDrawData* im_draw_data);

private:
    TextureHandle scene_color = nullptr;

    glm::uvec2 render_resolution;

    VkSampler bilinear_sampler;

    ImDrawData* imgui_draw_data = nullptr;

    BufferHandle index_buffer;
    BufferHandle vertex_buffer;

    void create_pipelines();

    void upscale_scene_color(CommandBuffer& commands, TextureHandle bloom_texture) const;

    void render_imgui_items(CommandBuffer& commands) const;

    GraphicsPipelineHandle upsample_pipeline;

    GraphicsPipelineHandle imgui_pipeline;
};
