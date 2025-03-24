#pragma once

#include <glm/vec2.hpp>

#include "core/halton_sequence.hpp"
#include "render/backend/handles.hpp"

struct GBuffer;
class RenderGraph;
class SceneView;

class IUpscaler {
public:
    virtual ~IUpscaler() = default;

    virtual void initialize(glm::uvec2 output_resolution, uint32_t frame_number) = 0;

    virtual glm::uvec2 get_optimal_render_resolution() const = 0;

    virtual void set_constants(const SceneView& scene_view, glm::uvec2 render_resolution) = 0;

    virtual glm::vec2 get_jitter();

    virtual void evaluate(
        RenderGraph& graph, const SceneView& view, const GBuffer& gbuffer, TextureHandle color_in,
        TextureHandle color_out, TextureHandle motion_vectors_in
    ) = 0;

private:
    HaltonSequence jitter_sequence_x = HaltonSequence{2};
    HaltonSequence jitter_sequence_y = HaltonSequence{3};
};
