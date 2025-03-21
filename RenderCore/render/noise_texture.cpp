#include "noise_texture.hpp"

#include <filesystem>

#include "texture_loader.hpp"
#include "spdlog/fmt/bundled/format.h"

NoiseTexture NoiseTexture::create(const std::string& base_filename, const uint32_t num_layers, TextureLoader& loader) {
    auto new_texture = NoiseTexture{
        .layers = std::vector<TextureHandle>(num_layers),
        .resolution = {128, 128},
        .num_layers = num_layers,
    };

    for(auto layer_idx = 0; layer_idx < num_layers; layer_idx++) {
        const auto filepath = std::filesystem::path{ fmt::format("{}_{}.png", base_filename, layer_idx) };
        const auto layer_texture = loader.load_texture(filepath, TextureType::Data);
        new_texture.layers[layer_idx] = *layer_texture;
    }

    return new_texture;
}

TextureHandle NoiseTexture::get_layer(const uint32_t index) const { return layers[index % num_layers]; }
