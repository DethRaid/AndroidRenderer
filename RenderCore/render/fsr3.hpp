#pragma once

#include <ffx_api/ffx_upscale.hpp>
#include <ffx_api/vk/ffx_api_vk.hpp>

#include <glm/vec2.hpp>

class RenderBackend;

class FidelityFSSuperResolution3
{
public:
    FidelityFSSuperResolution3(RenderBackend& backend);

    ~FidelityFSSuperResolution3();

    void initialize(const glm::uvec2& output_resolution);

    glm::uvec2 get_optimal_render_resolution() const;

private:
    bool has_context = false;
    ffx::Context upscaling_context;
    ffx::CreateBackendVKDesc backend_desc;

    glm::uvec2 optimal_render_resolution = {};
};

