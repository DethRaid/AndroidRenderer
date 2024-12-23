#pragma once

#include <glm/vec2.hpp>

#include "render/backend/compute_shader.hpp"
#include "render/backend/handles.hpp"

class RenderGraph;

class VRSAA
{
public:
    VRSAA();

    void measure_aliasing(RenderGraph& graph, TextureHandle lit_scene);

private:
    TextureHandle contrast_image = nullptr;

    VkSampler sampler;

    ComputePipelineHandle contrast_shader;

    void create_contrast_image(const glm::vec2& resolution);
};

