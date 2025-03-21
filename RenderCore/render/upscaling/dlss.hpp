#pragma once

#if SAH_USE_STREAMLINE

#include <glm/vec2.hpp>
#include <sl.h>
#include <sl_dlss.h>

#include "render/backend/handles.hpp"
#include "render/upscaling/upscaler.hpp"

class CommandBuffer;
class SceneView;
class RenderBackend;

class DLSSAdapter : public IUpscaler {
public:
    DLSSAdapter();

    ~DLSSAdapter() override;

    void initialize(glm::uvec2 output_resolution, uint32_t frame_index) override;

    glm::uvec2 get_optimal_render_resolution() const override;

    void set_constants(const SceneView& scene_transform, glm::uvec2 render_resolution) override;

    void evaluate(
        RenderGraph& graph,
        TextureHandle color_in, TextureHandle color_out,
        TextureHandle depth_in, TextureHandle motion_vectors_in
    ) override;

private:
    static inline bool available = false;

    sl::ViewportHandle viewport = {0};

    sl::DLSSMode dlss_mode = sl::DLSSMode::eDLAA;
    sl::DLSSOptimalSettings dlss_settings = {};

    sl::FrameToken* frame_token = nullptr;
};
#endif
