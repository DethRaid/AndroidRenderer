#pragma once

// Un-comment if you use the debug library
// #define KHRONOS_STATIC

#include <filesystem>
#include <unordered_map>

#include <tl/optional.hpp>
#include <volk.h>
#include <ktxvulkan.h>

#include "render/backend/handles.hpp"
#include "render/texture_type.hpp"
#include "render/backend/resource_allocator.hpp"
#include <spdlog/logger.h>

class RenderBackend;

/**
 * Loads textures and uploads them to the GPU
 */
class TextureLoader {
public:
    explicit TextureLoader(RenderBackend& backend_in);

    ~TextureLoader();

    tl::optional<TextureHandle> load_texture(const std::filesystem::path& filepath, TextureType type);

private:
    RenderBackend& backend;

    VkCommandPool ktx_command_pool;
    ktxVulkanDeviceInfo ktx;

    std::shared_ptr<spdlog::logger> logger;

    std::unordered_map<std::string, TextureHandle> loaded_textures;

    tl::optional<TextureHandle> load_texture_ktx(const std::filesystem::path& filepath, TextureType type);

    tl::optional<TextureHandle> load_texture_png(const std::filesystem::path& filepath, TextureType type);
};
