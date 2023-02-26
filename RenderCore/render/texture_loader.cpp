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

        const auto result = ktxVulkanDeviceInfo_Construct(&ktx, physical_device, device.device, queue, ktx_command_pool, nullptr);
        if (result != KTX_SUCCESS) {
            logger->error("Could not initialize KTX loader: {}", magic_enum::enum_name(result));
        }
    }
}

tl::optional<TextureHandle> TextureLoader::load_texture(const std::filesystem::path& filepath, TextureType type) {
    // Check if we already have the texture
    if (const auto itr = loaded_textures.find(filepath.string()); itr != loaded_textures.end()) {
        return itr->second;
    }

    // Load it form disk and upload it to the GPU if needed
    if (filepath.extension() == ".ktx" || filepath.extension() == ".ktx2") {
        return load_texture_ktx(filepath, type);
    } else {
        return load_texture_png(filepath, type);
    }
}


tl::optional<TextureHandle> TextureLoader::load_texture_ktx(const std::filesystem::path& filepath, TextureType type) {
    return SystemInterface::get()
            .load_file(filepath)
            .and_then([&](const std::vector<uint8_t>& data) -> tl::optional<TextureHandle> {
                ktxTexture* ktx_texture = nullptr;
                auto result = ktxTexture_CreateFromMemory(data.data(), data.size(),
                                                          KTX_TEXTURE_CREATE_NO_FLAGS,
                                                          &ktx_texture);
                if (result != KTX_SUCCESS) {
                    logger->error("Could not load file {}: {}", filepath.string(), magic_enum::enum_name(result));
                    return tl::nullopt;
                }

                auto texture = Texture{
                        .name = filepath.string(),
                        .type = TextureAllocationType::Ktx,
                };
                result = ktxTexture_VkUploadEx(ktx_texture, &ktx, &texture.ktx.ktx_vk_tex, VK_IMAGE_TILING_OPTIMAL,
                                               VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                if (result != KTX_SUCCESS) {
                    logger->error("Could not create Vulkan texture for KTX file {}: {}", filepath.string(), magic_enum::enum_name(result));
                    return tl::nullopt;
                }

                const auto& ktx_vk_tex = texture.ktx.ktx_vk_tex;

                texture.image = texture.ktx.ktx_vk_tex.image;
                texture.create_info =  VkImageCreateInfo{
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

                auto& allocator = backend.get_global_allocator();
                const auto handle = allocator.emplace_texture(filepath.string(), std::move(texture));
                loaded_textures.emplace(filepath.string(), handle);

                ktxTexture_Destroy(ktx_texture);

                return handle;
            });
}

tl::optional<TextureHandle> TextureLoader::load_texture_png(const std::filesystem::path& filepath, TextureType type) {
    return SystemInterface::get()
            .load_file(filepath)
            .and_then([&](const std::vector<uint8_t>& data) -> tl::optional<LoadedTexture> {
                          LoadedTexture texture;
                          int num_components;
                          const auto decoded_data = stbi_load_from_memory(
                                  data.data(), static_cast<int>(data.size()),
                                  &texture.width, &texture.height, &num_components, 4
                          );
                          if (decoded_data == nullptr) {
                              return tl::nullopt;
                          }

                          texture.data = std::vector(
                                  reinterpret_cast<uint8_t*>(decoded_data),
                                  reinterpret_cast<uint8_t*>(decoded_data) +
                                  texture.width * texture.height * 4
                          );

                          stbi_image_free(decoded_data);

                          return texture;
                      }
            )
            .and_then([&](const LoadedTexture& texture) -> tl::optional<TextureHandle> {
                          const auto format = [&]() {
                              switch (type) {
                                  case TextureType::Color:
                                      return VK_FORMAT_R8G8B8A8_SRGB;

                                  case TextureType::Data:
                                      return VK_FORMAT_R8G8B8A8_UNORM;
                              }
                          }();
                          auto& allocator = backend.get_global_allocator();
                          const auto handle = allocator.create_texture(
                                  filepath.string(), format,
                                  glm::uvec2{texture.width, texture.height}, 1,
                                  TextureUsage::StaticImage
                          );
                          loaded_textures.emplace(filepath.string(), handle);

                          auto& upload_queue = backend.get_upload_queue();
                          upload_queue.enqueue(
                                  TextureUploadJob{
                                          .destination = handle,
                                          .mip = 0,
                                          .data = texture.data,
                                  }
                          );

                          return handle;
                      }
            );
}

TextureLoader::~TextureLoader() {
    ktxVulkanDeviceInfo_Destruct(&ktx);
}
