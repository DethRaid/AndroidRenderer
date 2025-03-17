#pragma once

#if SAH_USE_XESS
#include <xess/xess.h>
#include <glm/vec2.hpp>

#include "render/upscaling/upscaler.hpp"

/**
 * Streamline-compatible interface for XeSS
 */
class XeSSAdapter : public IUpscaler
{
public:
    ~XeSSAdapter() override;

    void initialize(glm::uvec2 output_resolution, uint32_t frame_index) override;

    glm::uvec2 get_optimal_render_resolution() const override;

    void set_constants(const SceneView& scene_view, glm::uvec2 render_resolution) override;

     void evaluate(
        RenderGraph& graph, TextureHandle color_in, TextureHandle color_out, TextureHandle depth_in,
        TextureHandle motion_vectors_in
    ) override;
};

#endif
