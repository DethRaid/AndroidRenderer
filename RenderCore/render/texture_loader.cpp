#include "texture_loader.hpp"

#include <stb_image.h>
#include <magic_enum.hpp>

#include "core/system_interface.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/resource_upload_queue.hpp"

struct LoadedTexture {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> data = {};
};

TextureLoader::TextureLoader(RenderBackend& backend_in) : backend{backend_in} {
    logger = SystemInterface::get().get_logger("TextureLoader");

    {
        const auto physical_device = backend.get_physical_device();
        const auto device = backend.get_device();
        const auto queue = backend.get_transfer_queue();

        const auto command_pool_create_info = VkCommandPoolCreateInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = backend.get_transfer_queue_family_index()
        };

        vkCreateCommandPool(device.device, &command_pool_create_info, nullptr, &ktx_command_pool);

        const auto result = ktxVulkanDeviceInfo_ConstructEx(
            &ktx, backend.get_instance(), physical_device, device.device, queue, ktx_command_pool, nullptr, nullptr
        );
        if (result != KTX_SUCCESS) {
            logger->error("Could not initialize KTX loader: {}", magic_enum::enum_name(result));
        }
    }
}

TextureLoader::~TextureLoader() {
    ktxVulkanDeviceInfo_Destruct(&ktx);
}

tl::optional<TextureHandle> TextureLoader::load_texture(const std::filesystem::path& filepath, const TextureType type) {
    // Check if we already have the texture
    if (const auto itr = loaded_textures.find(filepath.string()); itr != loaded_textures.end()) {
        return itr->second;
    }

    // Load it form disk and upload it to the GPU if needed
    if (filepath.extension() == ".ktx" || filepath.extension() == ".ktx2") {
        return load_texture_ktx(filepath, type);
    } else {
        return load_texture_stbi(filepath, type);
    }
}


tl::optional<TextureHandle> TextureLoader::load_texture_ktx(
    const std::filesystem::path& filepath, const TextureType type
) {
    ZoneScoped;

    return SystemInterface::get()
           .load_file(filepath)
           .and_then([&](const std::vector<uint8_t>& data) { return upload_texture_ktx(filepath, data); });
}

tl::optional<TextureHandle> TextureLoader::load_texture_stbi(
    const std::filesystem::path& filepath, const TextureType type
) {
    ZoneScoped;

    return SystemInterface::get()
           .load_file(filepath)
           .and_then(
               [&](const std::vector<uint8_t>& data) { return upload_texture_stbi(filepath, data, type); }
           );
}

tl::optional<TextureHandle> TextureLoader::upload_texture_ktx(
    const std::filesystem::path& filepath, const std::vector<uint8_t>& data
) {
    ZoneScoped;

    ktxTexture2* ktx_texture = nullptr;
    auto result = ktxTexture2_CreateFromMemory(
        data.data(), data.size(),
        KTX_TEXTURE_CREATE_NO_FLAGS,
        &ktx_texture
    );
    if (result != KTX_SUCCESS) {
        logger->error("Could not load file {}: {}", filepath.string(), magic_enum::enum_name(result));
        return tl::nullopt;
    }

    if (ktxTexture2_NeedsTranscoding(ktx_texture)) {
        auto format = KTX_TTF_RGBA4444;
        if (backend.supports_astc()) {
            format = KTX_TTF_ASTC_4x4_RGBA;
        } else if (backend.supports_etc2()) {
            format = KTX_TTF_ETC2_RGBA;
        } else if (backend.supports_bc()) {
            format = KTX_TTF_BC7_RGBA;
        }
        ktxTexture2_TranscodeBasis(ktx_texture, format, 0);
    }

    auto texture = Texture{
        .name = filepath.string(),
        .type = TextureAllocationType::Ktx,
    };
    result = ktxTexture2_VkUpload(ktx_texture, &ktx, &texture.ktx.ktx_vk_tex);
    if (result != KTX_SUCCESS) {
        logger->error(
            "Could not create Vulkan texture for KTX file {}: {}", filepath.string(),
            magic_enum::enum_name(result)
        );
        return tl::nullopt;
    }

    const auto& ktx_vk_tex = texture.ktx.ktx_vk_tex;

    texture.image = texture.ktx.ktx_vk_tex.image;
    texture.create_info = VkImageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = ktx_vk_tex.imageFormat,
        .extent = {
            .width = ktx_vk_tex.width, .height = ktx_vk_tex.height, .depth = ktx_vk_tex.depth
        },
        .mipLevels = ktx_vk_tex.levelCount,
        .arrayLayers = ktx_vk_tex.layerCount,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
    };
    
    if (backend.has_separate_transfer_queue()) {
        backend.add_transfer_barrier(
            VkImageMemoryBarrier2{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamilyIndex = backend.get_transfer_queue_family_index(),
                .dstQueueFamilyIndex = backend.get_graphics_queue_family_index(),
                .image = texture.image,
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = ktx_vk_tex.levelCount,
                    .baseArrayLayer = 0,
                    .layerCount = ktx_vk_tex.layerCount
                }
            }
        );

        logger->info("Added queue transfer barrier for KTX image {} (Vulkan handle {})", filepath.string(), static_cast<void*>(texture.image));
    }

    ktxTexture_Destroy(ktxTexture(ktx_texture));
    
    auto& allocator = backend.get_global_allocator();
    const auto handle = allocator.emplace_texture(filepath.string(), std::move(texture));
    loaded_textures.emplace(filepath.string(), handle);

    return handle;
}

tl::optional<TextureHandle> TextureLoader::upload_texture_stbi(
    const std::filesystem::path& filepath, const std::vector<uint8_t>& data, const TextureType type
) {
    ZoneScoped;

    LoadedTexture loaded_texture;
    int num_components;
    const auto decoded_data = stbi_load_from_memory(
        data.data(), static_cast<int>(data.size()),
        &loaded_texture.width, &loaded_texture.height, &num_components, 4
    );
    if (decoded_data == nullptr) {
        return tl::nullopt;
    }

    loaded_texture.data = std::vector(
        reinterpret_cast<uint8_t*>(decoded_data),
        reinterpret_cast<uint8_t*>(decoded_data) +
        loaded_texture.width * loaded_texture.height * 4
    );

    stbi_image_free(decoded_data);

    const auto format = [&]() {
        switch (type) {
        case TextureType::Color:
            return VK_FORMAT_R8G8B8A8_SRGB;

        case TextureType::Data:
            return VK_FORMAT_R8G8B8A8_UNORM;
        }

        return VK_FORMAT_R8G8B8A8_UNORM;
    }();
    auto& allocator = backend.get_global_allocator();
    const auto handle = allocator.create_texture(
        filepath.string(), format,
        glm::uvec2{loaded_texture.width, loaded_texture.height}, 1,
        TextureUsage::StaticImage
    );
    loaded_textures.emplace(filepath.string(), handle);

    auto& upload_queue = backend.get_upload_queue();
    upload_queue.enqueue(
        TextureUploadJob{
            .destination = handle,
            .mip = 0,
            .data = loaded_texture.data,
        }
    );
        
    if (backend.has_separate_transfer_queue()) {
        const auto& texture = allocator.get_texture(handle);

        backend.add_transfer_barrier(
            VkImageMemoryBarrier2{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamilyIndex = backend.get_transfer_queue_family_index(),
                .dstQueueFamilyIndex = backend.get_graphics_queue_family_index(),
                .image = texture.image,
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            }
        );

        logger->info("Added queue transfer barrier for image {} (Vulkan handle {})", filepath.string(), static_cast<void*>(texture.image));
    }

    return handle;
}
