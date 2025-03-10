#pragma once

#include <string>
#include <vector>
#include <glm/vec2.hpp>

#include "backend/handles.hpp"

class TextureLoader;

struct NoiseTexture
{
    static NoiseTexture create(const std::string& base_filename, uint32_t num_layers, TextureLoader& loader);

    std::vector<TextureHandle> layers;

    glm::uvec2 resolution = {};

    uint32_t num_layers = {};
};

