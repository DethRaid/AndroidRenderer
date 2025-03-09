#pragma once

#if SAH_USE_FFX

#include <ffx_api/ffx_upscale.hpp>
#include <ffx_api/vk/ffx_api_vk.hpp>

#include <glm/vec2.hpp>

#include "backend/handles.hpp"

class SceneView;
class CommandBuffer;
class RenderBackend;

class FidelityFSSuperResolution3 {
public:
    FidelityFSSuperResolution3(RenderBackend& backend);

    ~FidelityFSSuperResolution3();

    void initialize(const glm::uvec2& output_resolution);

    void set_constants(const SceneView& scene_transform, glm::uvec2 render_resolution);

    void dispatch(
        const CommandBuffer& commands, TextureHandle color_in, TextureHandle color_out, TextureHandle depth_in,
        TextureHandle motion_vectors_in
    );

    glm::uvec2 get_optimal_render_resolution() const;

    glm::vec2 get_jitter() const;

private:
    bool has_context = false;
    ffx::Context upscaling_context;
    ffx::CreateBackendVKDesc backend_desc;

    glm::uvec2 optimal_render_resolution = {};
    glm::uvec2 output_resolution = {};

    int32_t jitter_index = 0;
    glm::vec2 jitter = {};

    ffx::DispatchDescUpscale dispatch_desc = {};
};

#endif
