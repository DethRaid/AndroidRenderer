#pragma once

#if SAH_USE_FFX

#include <ffx_api/ffx_upscale.hpp>
#include <ffx_api/vk/ffx_api_vk.hpp>

#include <glm/vec2.hpp>

#include "render/upscaling/upscaler.hpp"
#include "render/backend/handles.hpp"

class SceneView;
class CommandBuffer;
class RenderBackend;

class FidelityFSSuperResolution3 : public IUpscaler {
public:
    FidelityFSSuperResolution3();

    ~FidelityFSSuperResolution3() override;

    void initialize(glm::uvec2 output_resolution_in, uint32_t frame_number) override;

    void set_constants(const SceneView& scene_transform, glm::uvec2 render_resolution) override;

    void evaluate(
        RenderGraph& graph, const SceneView& view, const GBuffer& gbuffer, TextureHandle color_in,
        TextureHandle color_out, TextureHandle motion_vectors_in
    ) override;

    glm::uvec2 get_optimal_render_resolution() const override;

    glm::vec2 get_jitter() override;

private:
    bool has_context = false;
    ffx::Context upscaling_context = nullptr;
    ffx::CreateBackendVKDesc backend_desc;

    glm::uvec2 optimal_render_resolution = {};
    glm::uvec2 output_resolution = {};

    int32_t jitter_index = 0;
    glm::vec2 jitter = {};

    ffx::DispatchDescUpscale dispatch_desc = {};
};

#endif
