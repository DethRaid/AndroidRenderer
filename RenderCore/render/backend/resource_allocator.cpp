#include "resource_allocator.hpp"

#include "render/backend/utils.hpp"
#include "render/backend/render_backend.hpp"

#include <spdlog/fmt/bundled/format.h>

ResourceAllocator::ResourceAllocator(RenderBackend& backend_in) :
    backend{backend_in} {
    const auto functions = VmaVulkanFunctions{
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
    };
    const auto create_info = VmaAllocatorCreateInfo{
        .physicalDevice = backend.get_physical_device(),
        .device = backend.get_device().device,
        .pVulkanFunctions = &functions,
        .instance = backend.get_instance(),
        .vulkanApiVersion = VK_API_VERSION_1_1
    };
    const auto result = vmaCreateAllocator(&create_info, &vma);
    if (result != VK_SUCCESS) {
        throw std::runtime_error{"Could not create VMA instance"};
    }
}

ResourceAllocator::~ResourceAllocator() {
    const auto device = backend.get_device().device;
    for (const auto& [info, sampler] : sampler_cache) {
        vkDestroySampler(device, sampler, nullptr);
    }
}


TextureHandle ResourceAllocator::create_texture(
    const std::string& name, VkFormat format, glm::uvec2 resolution,
    const uint32_t num_mips, const TextureUsage usage,
    const uint32_t num_layers, const VkFormat view_format
) {
    const auto device = backend.get_device().device;

    VkImageUsageFlags vk_usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    VmaAllocationCreateFlags vma_flags = {};
    VkImageAspectFlags view_aspect = VK_IMAGE_ASPECT_COLOR_BIT;

    switch (usage) {
    case TextureUsage::RenderTarget:
        if (is_depth_format(format)) {
            vk_usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
            view_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        } else {
            vk_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        }
        vma_flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        break;

    case TextureUsage::StaticImage:
        vk_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        break;

    case TextureUsage::StorageImage:
        vk_usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        break;
    }

    const auto image_create_info = VkImageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = VkExtent3D{.width = resolution.x, .height = resolution.y, .depth = 1},
        .mipLevels = num_mips,
        .arrayLayers = num_layers,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = vk_usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    const auto allocation_info = VmaAllocationCreateInfo{
        .flags = vma_flags,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };

    auto texture = Texture{
        .type = TextureAllocationType::Vma
    };

    auto result = vmaCreateImage(
        vma, &image_create_info, &allocation_info, &texture.image, &texture.vma.allocation,
        &texture.vma.allocation_info
    );
    if (result != VK_SUCCESS) {
        throw std::runtime_error{fmt::format("Could not create image {}", name)};
    }

    const auto name_info = VkDebugUtilsObjectNameInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_IMAGE,
        .objectHandle = reinterpret_cast<uint64_t>(texture.image),
        .pObjectName = name.c_str(),
    };
    vkSetDebugUtilsObjectNameEXT(device, &name_info);

    texture.name = name;
    texture.create_info = image_create_info;

    const auto image_view_name = fmt::format("{} View", name);

    const auto view_create_info = VkImageViewCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = texture.image,
        .viewType = num_layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
        .format = view_format == VK_FORMAT_UNDEFINED ? format : view_format,
        .subresourceRange = {
            .aspectMask = view_aspect,
            .baseMipLevel = 0,
            .levelCount = num_mips,
            .baseArrayLayer = 0,
            .layerCount = num_layers,
        },
    };
    result = vkCreateImageView(device, &view_create_info, nullptr, &texture.image_view);
    if (result != VK_SUCCESS) {
        throw std::runtime_error{fmt::format("Could not create image view {}", image_view_name)};
    }

    const auto view_name_info = VkDebugUtilsObjectNameInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_IMAGE_VIEW,
        .objectHandle = reinterpret_cast<uint64_t>(texture.image_view),
        .pObjectName = image_view_name.c_str(),
    };
    vkSetDebugUtilsObjectNameEXT(device, &view_name_info);

    texture.rtv = texture.image_view;

    auto handle = textures.add_object(std::move(texture));
    return static_cast<TextureHandle>(handle.index);
}

TextureHandle ResourceAllocator::create_volume_texture(
    const std::string& name, VkFormat format, glm::uvec3 resolution,
    uint32_t num_mips, TextureUsage usage
) {
    const auto device = backend.get_device().device;

    VkImageUsageFlags vk_usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    VmaAllocationCreateFlags vma_flags = {};
    VkImageAspectFlags view_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    VkImageCreateFlags image_create_flags = {};

    switch (usage) {
    case TextureUsage::RenderTarget:
        if (is_depth_format(format)) {
            vk_usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
            view_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        } else {
            vk_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        }
        image_create_flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
        vma_flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        break;

    case TextureUsage::StaticImage:
        vk_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        break;

    case TextureUsage::StorageImage:
        vk_usage |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        image_create_flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
        break;
    }

    const auto image_create_info = VkImageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = image_create_flags,
        .imageType = VK_IMAGE_TYPE_3D,
        .format = format,
        .extent = VkExtent3D{.width = resolution.x, .height = resolution.y, .depth = resolution.z},
        .mipLevels = num_mips,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = vk_usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    const auto allocation_info = VmaAllocationCreateInfo{
        .flags = vma_flags,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };

    auto texture = Texture{
        .type = TextureAllocationType::Vma
    };

    auto result = vmaCreateImage(
        vma, &image_create_info, &allocation_info, &texture.image, &texture.vma.allocation,
        &texture.vma.allocation_info
    );
    if (result != VK_SUCCESS) {
        throw std::runtime_error{fmt::format("Could not create image {}", name)};
    }

    const auto name_info = VkDebugUtilsObjectNameInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_IMAGE,
        .objectHandle = reinterpret_cast<uint64_t>(texture.image),
        .pObjectName = name.c_str(),
    };
    vkSetDebugUtilsObjectNameEXT(device, &name_info);

    texture.name = name;
    texture.create_info = image_create_info;

    const auto image_view_name = fmt::format("{} View", name);

    const auto view_create_info = VkImageViewCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = texture.image,
        .viewType = VK_IMAGE_VIEW_TYPE_3D,
        .format = format,
        .subresourceRange = {
            .aspectMask = view_aspect,
            .baseMipLevel = 0,
            .levelCount = num_mips,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    result = vkCreateImageView(device, &view_create_info, nullptr, &texture.image_view);
    if (result != VK_SUCCESS) {
        throw std::runtime_error{fmt::format("Could not create image view {}", image_view_name)};
    }

    const auto view_name_info = VkDebugUtilsObjectNameInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_IMAGE_VIEW,
        .objectHandle = reinterpret_cast<uint64_t>(texture.image_view),
        .pObjectName = image_view_name.c_str(),
    };
    vkSetDebugUtilsObjectNameEXT(device, &view_name_info);

    const auto rtv_name = fmt::format("{} RTV", name);

    const auto rtv_create_info = VkImageViewCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = texture.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        .format = format,
        .subresourceRange = {
            .aspectMask = view_aspect,
            .baseMipLevel = 0,
            .levelCount = num_mips,
            .baseArrayLayer = 0,
            .layerCount = resolution.z,
        },
    };
    result = vkCreateImageView(device, &rtv_create_info, nullptr, &texture.rtv);
    if (result != VK_SUCCESS) {
        throw std::runtime_error{fmt::format("Could not create image view {}", image_view_name)};
    }

    const auto rtv_name_info = VkDebugUtilsObjectNameInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_IMAGE_VIEW,
        .objectHandle = reinterpret_cast<uint64_t>(texture.rtv),
        .pObjectName = image_view_name.c_str(),
    };
    vkSetDebugUtilsObjectNameEXT(device, &rtv_name_info);

    auto handle = textures.add_object(std::move(texture));
    return static_cast<TextureHandle>(handle.index);
}

TextureHandle ResourceAllocator::emplace_texture(const std::string& name, Texture&& new_texture) {
    if (new_texture.type == TextureAllocationType::Ktx) {
        // Name the image, create an image view, name the image view

        const auto device = backend.get_device().device;

        const auto name_info = VkDebugUtilsObjectNameInfoEXT{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = VK_OBJECT_TYPE_IMAGE,
            .objectHandle = reinterpret_cast<uint64_t>(new_texture.image),
            .pObjectName = name.c_str(),
        };
        vkSetDebugUtilsObjectNameEXT(device, &name_info);

        const auto image_view_name = fmt::format("{} View", name);

        if (new_texture.image_view == VK_NULL_HANDLE) {
            VkImageAspectFlags view_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
            if (is_depth_format(new_texture.create_info.format)) {
                view_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
            }

            const auto view_create_info = VkImageViewCreateInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = new_texture.image,
                .viewType = new_texture.ktx.ktx_vk_tex.viewType,
                .format = new_texture.create_info.format,
                .subresourceRange = {
                    .aspectMask = view_aspect,
                    .baseMipLevel = 0,
                    .levelCount = new_texture.create_info.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount = new_texture.create_info.arrayLayers,
                },
            };
            const auto result = vkCreateImageView(device, &view_create_info, nullptr, &new_texture.image_view);
            if (result != VK_SUCCESS) {
                throw std::runtime_error{fmt::format("Could not create image view {}", image_view_name)};
            }
        }

        const auto view_name_info = VkDebugUtilsObjectNameInfoEXT{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = VK_OBJECT_TYPE_IMAGE_VIEW,
            .objectHandle = reinterpret_cast<uint64_t>(new_texture.image_view),
            .pObjectName = image_view_name.c_str(),
        };
        vkSetDebugUtilsObjectNameEXT(device, &view_name_info);
    }

    if(new_texture.rtv == VK_NULL_HANDLE) {
        new_texture.rtv = new_texture.image_view;
    }

    auto handle = textures.add_object(std::move(new_texture));
    return static_cast<TextureHandle>(handle.index);
}

const Texture& ResourceAllocator::get_texture(TextureHandle handle) const {
    return textures[static_cast<uint32_t>(handle)];
}

void ResourceAllocator::destroy_texture(TextureHandle handle) {
    auto& cur_frame_zombies = texture_zombie_lists[backend.get_current_gpu_frame()];
    cur_frame_zombies.emplace_back(handle);
}

BufferHandle ResourceAllocator::create_buffer(const std::string& name, size_t size, BufferUsage usage) {
    const auto device = backend.get_device().device;

    VkBufferUsageFlags vk_usage = {};
    VmaAllocationCreateFlags vma_flags = {};
    VmaMemoryUsage memory_usage = {};

    switch (usage) {
    case BufferUsage::StagingBuffer:
        vk_usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        vma_flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        break;

    case BufferUsage::VertexBuffer:
        vk_usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        break;

    case BufferUsage::IndexBuffer:
        vk_usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        break;

    case BufferUsage::IndirectBuffer:
        vk_usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        break;

    case BufferUsage::UniformBuffer:
        vk_usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        vma_flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        break;

    case BufferUsage::StorageBuffer:
        vk_usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        break;
    }
    const auto create_info = VkBufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = vk_usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    const auto vma_create_info = VmaAllocationCreateInfo{
        .flags = vma_flags,
        .usage = memory_usage,
    };

    Buffer buffer;
    auto result = vmaCreateBuffer(
        vma, &create_info, &vma_create_info, &buffer.buffer, &buffer.allocation,
        &buffer.allocation_info
    );
    if (result != VK_SUCCESS) {
        throw std::runtime_error{fmt::format("Could not create buffer {}", name)};
    }

    const auto name_info = VkDebugUtilsObjectNameInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_BUFFER,
        .objectHandle = reinterpret_cast<uint64_t>(buffer.buffer),
        .pObjectName = name.c_str(),
    };
    vkSetDebugUtilsObjectNameEXT(device, &name_info);

    buffer.name = name;
    buffer.create_info = create_info;

    const auto handle = buffers.add_object(std::move(buffer));
    return static_cast<BufferHandle>(handle.index);
}

const Buffer& ResourceAllocator::get_buffer(BufferHandle handle) const {
    return buffers[static_cast<uint32_t>(handle)];
}

void ResourceAllocator::destroy_buffer(BufferHandle handle) {
    auto& cur_frame_zombies = buffer_zombie_lists[backend.get_current_gpu_frame()];
    cur_frame_zombies.emplace_back(handle);
}

void ResourceAllocator::destroy_framebuffer(Framebuffer&& framebuffer) {
    auto& cur_frame_zombies = framebuffer_zombie_lists[backend.get_current_gpu_frame()];
    cur_frame_zombies.emplace_back(framebuffer);
}

VkSampler ResourceAllocator::get_sampler(const VkSamplerCreateInfo& info) {
    const auto device = backend.get_device().device;

    const auto info_hash = SamplerCreateInfoHasher{}(info);

    if (const auto& itr = sampler_cache.find(info_hash); itr != sampler_cache.end()) {
        return itr->second;
    }

    VkSampler sampler;
    vkCreateSampler(device, &info, nullptr, &sampler);

    sampler_cache.emplace(info_hash, sampler);

    return sampler;
}

void ResourceAllocator::free_resources_for_frame(const uint32_t frame_idx) {
    ZoneScoped;

    auto& zombie_buffers = buffer_zombie_lists[frame_idx];
    for (auto handle : zombie_buffers) {
        auto& buffer = buffers[static_cast<uint32_t>(handle)];
        vmaDestroyBuffer(vma, buffer.buffer, buffer.allocation);
        buffers.free_object(static_cast<uint32_t>(handle));
    }

    zombie_buffers.clear();

    auto& zombie_textures = texture_zombie_lists[frame_idx];
    const auto device = backend.get_device().device;
    for (auto handle : zombie_textures) {
        auto& texture = textures[static_cast<uint32_t>(handle)];
        vkDestroyImageView(device, texture.image_view, nullptr);

        switch (texture.type) {
        case TextureAllocationType::Vma:
            vmaDestroyImage(vma, texture.image, texture.vma.allocation);
            break;

        case TextureAllocationType::Ktx:
            ktxVulkanTexture_Destruct(&texture.ktx.ktx_vk_tex, device, nullptr);
            break;

        case TextureAllocationType::Swapchain:
            // We just need to destroy the image view
            vkDestroyImageView(device, texture.image_view, nullptr);
            break;

        default:
            throw std::runtime_error{"Unknown texture allocation type"};
        }
    }

    zombie_textures.clear();

    auto& zombie_framebuffers = framebuffer_zombie_lists[frame_idx];
    for (const auto& framebuffer : zombie_framebuffers) {
        vkDestroyFramebuffer(device, framebuffer.framebuffer, nullptr);
    }
    zombie_framebuffers.clear();
}

VmaAllocator ResourceAllocator::get_vma() const {
    return vma;
}
