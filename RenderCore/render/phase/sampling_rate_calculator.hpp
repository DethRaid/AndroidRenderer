#pragma once

#include <glm/vec2.hpp>

#include "render/backend/compute_shader.hpp"
#include "render/backend/handles.hpp"

class RenderGraph;

class VRSAA
{
public:
    VRSAA();

    void init(const glm::uvec2& resolution);

    void generate_shading_rate_image(RenderGraph& graph) const;

    void measure_aliasing(RenderGraph& graph, TextureHandle lit_scene) const;

private:
    TextureHandle contrast_image = nullptr;

    TextureHandle shading_rate_image = nullptr;

    VkSampler sampler;

    ComputePipelineHandle generate_shading_rate_image_shader;

    BufferHandle params_buffer = nullptr;
    ComputePipelineHandle contrast_shader;

    void create_shading_rate_image(const glm::vec2& resolution);

    void create_contrast_image(const glm::vec2& resolution);

    void create_params_buffer();
};

