#pragma once

#include <string>
#include <EASTL/vector.h>
#include <glm/vec2.hpp>

#include "backend/handles.hpp"

class TextureLoader;

struct NoiseTexture
{
    static NoiseTexture create(const std::string& base_filename, uint32_t num_layers, TextureLoader& loader);

    eastl::vector<TextureHandle> layers;

    glm::uvec2 resolution = {};

    uint32_t num_layers = {};

    TextureHandle get_layer(uint32_t index) const;
};

