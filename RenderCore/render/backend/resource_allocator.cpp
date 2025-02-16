#include "resource_allocator.hpp"

#include "render/backend/utils.hpp"
#include "render/backend/render_backend.hpp"

#include <spdlog/fmt/bundled/format.h>
#include <tracy/Tracy.hpp>
#include <vulkan/vk_enum_string_helper.h>

#include "core/system_interface.hpp"
#include "render/backend/acceleration_structure.hpp"
#include "render/backend/gpu_texture.hpp"

static std::shared_ptr<spdlog::logger> logger;

ResourceAllocator::ResourceAllocator(RenderBackend& backend_in) :
    backend{backend_in} {
    if(logger == nullptr) {
        logger = SystemInterface::get().get_logger("ResourceAllocator");
        logger->set_level(spdlog::level::info);
    }
    const auto functions = VmaVulkanFunctions{
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
    };
    const auto create_info = VmaAllocatorCreateInfo{
        .flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = backend.get_physical_device(),
        .device = backend.get_device().device,
        .pVulkanFunctions = &functions,
        .instance = backend.get_instance(),
        .vulkanApiVersion = VK_API_VERSION_1_3
    };
    const auto result = vmaCreateAllocator(&create_info, &vma);
    if(result != VK_SUCCESS) {
        throw std::runtime_error{"Could not create VMA instance"};
    }
}

ResourceAllocator::~ResourceAllocator() {
    const auto device = backend.get_device().device;
    for(const auto& [info, sampler] : sampler_cache) {
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

    switch(usage) {
    case TextureUsage::RenderTarget:
        if(is_depth_format(format)) {
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

    case TextureUsage::ShadingRateImage:
        vk_usage = VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR | VK_IMAGE_USAGE_STORAGE_BIT;
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

    auto texture = GpuTexture{
        .type = TextureAllocationType::Vma
    };

    auto result = vmaCreateImage(
        vma,
        &image_create_info,
        &allocation_info,
        &texture.image,
        &texture.vma.allocation,
        &texture.vma.allocation_info
    );
    if(result != VK_SUCCESS) {
        throw std::runtime_error{fmt::format("Could not create image {}", name)};
    }

    texture.name = name;
    texture.create_info = image_create_info;

    const auto image_view_name = fmt::format("{} View", name);

    {
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
        if(result != VK_SUCCESS) {
            throw std::runtime_error{fmt::format("Could not create image view {}", image_view_name)};
        }
    }

    if(num_mips == 1) {
        texture.attachment_view = texture.image_view;
    } else {
        const auto rtv_create_info = VkImageViewCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = texture.image,
            .viewType = num_layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
            .format = view_format == VK_FORMAT_UNDEFINED ? format : view_format,
            .subresourceRange = {
                .aspectMask = view_aspect,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = num_layers,
            },
        };
        result = vkCreateImageView(device, &rtv_create_info, nullptr, &texture.attachment_view);
    }

    backend.set_object_name(texture.image, name);
    backend.set_object_name(texture.image_view, image_view_name);
    backend.set_object_name(texture.attachment_view, fmt::format("{} RTV", name));

    texture.mip_views.reserve(num_mips);
    for(auto i = 0u; i < num_mips; i++) {
        const auto view_create_info = VkImageViewCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = texture.image,
            .viewType = num_layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
            .format = view_format == VK_FORMAT_UNDEFINED ? format : view_format,
            .subresourceRange = {
                .aspectMask = view_aspect,
                .baseMipLevel = i,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = num_layers,
            },
        };
        auto view = VkImageView{};
        result = vkCreateImageView(device, &view_create_info, nullptr, &view);
        if(result != VK_SUCCESS) {
            throw std::runtime_error{fmt::format("Could not create image view")};
        }

        backend.set_object_name(view, fmt::format("{} mip {}", name, i));

        texture.mip_views.emplace_back(view);
    }

    auto handle = &(*textures.emplace(std::move(texture)));
    return handle;
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

    switch(usage) {
    case TextureUsage::RenderTarget:
        if(is_depth_format(format)) {
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

    default:
        throw std::runtime_error{"Unsupported 3D image usage"};
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

    auto texture = GpuTexture{
        .type = TextureAllocationType::Vma
    };

    auto result = vmaCreateImage(
        vma,
        &image_create_info,
        &allocation_info,
        &texture.image,
        &texture.vma.allocation,
        &texture.vma.allocation_info
    );
    if(result != VK_SUCCESS) {
        throw std::runtime_error{fmt::format("Could not create image {}", name)};
    }

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
    if(result != VK_SUCCESS) {
        throw std::runtime_error{fmt::format("Could not create image view {}", image_view_name)};
    }

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
    result = vkCreateImageView(device, &rtv_create_info, nullptr, &texture.attachment_view);
    if(result != VK_SUCCESS) {
        throw std::runtime_error{fmt::format("Could not create image view {} RTV", name)};
    }

    backend.set_object_name(texture.image, name);
    backend.set_object_name(texture.image_view, image_view_name);
    backend.set_object_name(texture.attachment_view, fmt::format("{} RTV", name));

    auto handle = &(*textures.emplace(std::move(texture)));
    return handle;
}

TextureHandle ResourceAllocator::emplace_texture(GpuTexture&& new_texture) {
    if(new_texture.type == TextureAllocationType::Ktx) {
        // Name the image, create an image view, name the image view

        const auto device = backend.get_device().device;

        if(new_texture.image_view == VK_NULL_HANDLE) {
            VkImageAspectFlags view_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
            if(is_depth_format(new_texture.create_info.format)) {
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
            if(result != VK_SUCCESS) {
                throw std::runtime_error{fmt::format("Could not create image view for image {}", new_texture.name)};
            }
        }
    }

    if(new_texture.attachment_view == VK_NULL_HANDLE) {
        new_texture.attachment_view = new_texture.image_view;
    }

    const auto image_view_name = fmt::format("{} View", new_texture.name);
    backend.set_object_name(new_texture.image, new_texture.name);
    backend.set_object_name(new_texture.image_view, image_view_name);

    auto handle = &(*textures.emplace(std::move(new_texture)));
    return handle;
}

void ResourceAllocator::destroy_texture(TextureHandle handle) {
    auto& cur_frame_zombies = texture_zombie_lists[backend.get_current_gpu_frame()];
    cur_frame_zombies.emplace_back(handle);
}

BufferHandle ResourceAllocator::create_buffer(const std::string& name, const size_t size, const BufferUsage usage) {
    logger->trace("Creating buffer {} with size {} and usage {}", name, size, to_string(usage));

    const auto device = backend.get_device().device;

    VkBufferUsageFlags vk_usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VmaAllocationCreateFlags vma_flags = {};
    VmaMemoryUsage memory_usage = {};

    switch(usage) {
    case BufferUsage::StagingBuffer:
        vk_usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if (backend.supports_ray_tracing()) {
            vk_usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
        }
        vma_flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        break;

    case BufferUsage::VertexBuffer:
        vk_usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if(backend.supports_ray_tracing()) {
            vk_usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
        }
        memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        break;

    case BufferUsage::IndexBuffer:
        vk_usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (backend.supports_ray_tracing()) {
            vk_usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
        }
        memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        break;

    case BufferUsage::IndirectBuffer:
        vk_usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        break;

    case BufferUsage::UniformBuffer:
        vk_usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        vma_flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        break;

    case BufferUsage::StorageBuffer:
        vk_usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        break;

    case BufferUsage::AccelerationStructure:
        vk_usage |= VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
            VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR | VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT_KHR;
        memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        break;
    }
    const auto create_info = VkBufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = std::max(size, static_cast<size_t>(256)),
        .usage = vk_usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    const auto vma_create_info = VmaAllocationCreateInfo{
        .flags = vma_flags,
        .usage = memory_usage,
    };

    GpuBuffer buffer;
    auto result = vmaCreateBuffer(
        vma,
        &create_info,
        &vma_create_info,
        &buffer.buffer,
        &buffer.allocation,
        &buffer.allocation_info
    );
    if(result != VK_SUCCESS) {
        throw std::runtime_error{fmt::format("Could not create buffer {}", name)};
    }

    backend.set_object_name(buffer.buffer, name);

    buffer.name = name;
    buffer.create_info = create_info;

    const auto info = VkBufferDeviceAddressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer.buffer
    };
    buffer.address = vkGetBufferDeviceAddress(device, &info);

    auto handle = &(*buffers.emplace(std::move(buffer)));
    return handle;
}

void* ResourceAllocator::map_buffer(const BufferHandle buffer_handle) const {
    if(buffer_handle->allocation_info.pMappedData == nullptr) {
        vmaMapMemory(vma, buffer_handle->allocation, &buffer_handle->allocation_info.pMappedData);
    }

    assert(buffer_handle->allocation_info.pMappedData != nullptr);

    return buffer_handle->allocation_info.pMappedData;
}

AccelerationStructureHandle ResourceAllocator::create_acceleration_structure(
    const uint64_t acceleration_structure_size, VkAccelerationStructureTypeKHR type
) {
    ZoneScoped;
    AccelerationStructure as;

    as.buffer = create_buffer(
        "Acceleration structure",
        acceleration_structure_size,
        BufferUsage::AccelerationStructure);

    const auto acceleration_structure_create_info = VkAccelerationStructureCreateInfoKHR{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = as.buffer->buffer,
        .size = acceleration_structure_size,
        .type = type,
    };
    vkCreateAccelerationStructureKHR(
        backend.get_device(),
        &acceleration_structure_create_info,
        nullptr,
        &as.acceleration_structure);

    const auto acceleration_device_address_info = VkAccelerationStructureDeviceAddressInfoKHR{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = as.acceleration_structure,
    };
    as.as_address = vkGetAccelerationStructureDeviceAddressKHR(
        backend.get_device(),
        &acceleration_device_address_info);

    auto handle = &(*acceleration_structures.emplace(as));
    return handle;
}

void ResourceAllocator::destroy_acceleration_structure(AccelerationStructureHandle handle) {
    as_zombie_lists[backend.get_current_gpu_frame()].emplace_back(handle);
    destroy_buffer(handle->buffer);
}

void* ResourceAllocator::map_buffer(const BufferHandle buffer_handle) {
    if (buffer_handle->allocation_info.pMappedData == nullptr) {
        vmaMapMemory(vma, buffer_handle->allocation, &buffer_handle->allocation_info.pMappedData);
    }

    return buffer_handle->allocation_info.pMappedData;
}

AccelerationStructureHandle ResourceAllocator::create_acceleration_structure() {
    return {};
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

    if(const auto& itr = sampler_cache.find(info_hash); itr != sampler_cache.end()) {
        return itr->second;
    }

    VkSampler sampler;
    vkCreateSampler(device, &info, nullptr, &sampler);

    sampler_cache.emplace(info_hash, sampler);

    return sampler;
}

VkRenderPass ResourceAllocator::get_render_pass(const RenderPass& pass) {
    ZoneScoped;

    if(const auto itr = cached_render_passes.find(pass.name); itr != cached_render_passes.end()) {
        return itr->second;
    }

    logger->debug("Creating render pass {}", pass.name);

    const auto total_num_attachments = pass.attachments.size();

    auto attachments = std::vector<VkAttachmentDescription2>{};
    attachments.reserve(total_num_attachments);

    {
        auto attachment_index = 0u;

        for (const auto& render_target : pass.attachments) {
            auto load_action = VK_ATTACHMENT_LOAD_OP_LOAD;
            auto store_action = VK_ATTACHMENT_STORE_OP_STORE;
            if (render_target->is_transient) {
                load_action = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                store_action = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            }
            if (pass.clear_values.size() > attachment_index) {
                load_action = VK_ATTACHMENT_LOAD_OP_CLEAR;
            }

            auto layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            if (is_depth_format(render_target->create_info.format)) {
                layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }

            logger->debug("RenderPass attachment {} is {}", attachments.size(), render_target->name);
            logger->debug(
                "\tloadOp={} initialLayout={}",
                string_VkAttachmentLoadOp(load_action),
                string_VkImageLayout(layout)
            );
            logger->debug(
                "\tstoreOp={} finalLayout={}",
                string_VkAttachmentStoreOp(store_action),
                string_VkImageLayout(layout)
            );

            attachments.emplace_back(
                VkAttachmentDescription2{
                    .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
                    .format = render_target->create_info.format,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = load_action,
                    .storeOp = store_action,
                    .initialLayout = layout,
                    .finalLayout = layout,
                }
                );

            attachment_index++;
        }
    }

    auto attachment_references = std::vector<std::vector<VkAttachmentReference2>>{};
    auto subpasses = std::vector<VkSubpassDescription2>{};
    auto dependencies = std::vector<VkSubpassDependency2>{};

    attachment_references.reserve(pass.subpasses.size() * 3);
    subpasses.reserve(pass.subpasses.size());
    dependencies.reserve(pass.subpasses.size());

    for(auto subpass_index = 0u; subpass_index < pass.subpasses.size(); subpass_index++) {
        const auto& subpass = pass.subpasses[subpass_index];
        auto description = VkSubpassDescription2{
            .sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        };

        if(!subpass.input_attachments.empty()) {
            auto& input_attachment_references = attachment_references.emplace_back();
            input_attachment_references.reserve(subpass.input_attachments.size());
            for(const auto& input_attachment_index : subpass.input_attachments) {
                const auto input_attachment_handle = pass.attachments[input_attachment_index];
                if(is_depth_format(input_attachment_handle->create_info.format)) {
                    input_attachment_references.emplace_back(
                        VkAttachmentReference2{
                            .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                            .attachment = input_attachment_index,
                            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT
                        }
                    );
                } else {
                    input_attachment_references.emplace_back(
                        VkAttachmentReference2{
                            .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                            .attachment = input_attachment_index,
                            .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
                        }
                    );
                }
            }
            description.inputAttachmentCount = static_cast<uint32_t>(input_attachment_references.size());
            description.pInputAttachments = input_attachment_references.data();
        }

        if(!subpass.color_attachments.empty()) {
            auto& color_attachment_references = attachment_references.emplace_back();
            color_attachment_references.reserve(subpass.color_attachments.size());
            for(const auto& color_attachment_index : subpass.color_attachments) {
                color_attachment_references.emplace_back(
                    VkAttachmentReference2{
                        .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                        .attachment = color_attachment_index,
                        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
                    }
                );
            }
            description.colorAttachmentCount = static_cast<uint32_t>(color_attachment_references.size());
            description.pColorAttachments = color_attachment_references.data();
        }

        if(subpass.depth_attachment) {
            auto& depth_attachment_references = attachment_references.emplace_back();
            depth_attachment_references.reserve(1);

            depth_attachment_references.emplace_back(
                VkAttachmentReference2{
                    .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                    .attachment = *subpass.depth_attachment,
                    .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT
                }
            );

            description.pDepthStencilAttachment = depth_attachment_references.data();
        }

        pass.view_mask.map(
            [&](const uint32_t view_mask) {
                description.viewMask = view_mask;
            }
        );

        subpasses.emplace_back(description);

        if(subpass_index != 0 && !subpass.input_attachments.empty()) {
            // Find the previous subpass that produces this subpass's input attachments, and add a dependency between them

            // Copy the input attachments to a new array so we can remove from the new array
            auto input_attachments_unproduced = subpass.input_attachments;

            for(auto producer_index = static_cast<int32_t>(subpass_index - 1);
                producer_index >= 0; producer_index--) {
                const auto& previous_subpass = pass.subpasses[producer_index];

                // If the previous subpass produces any of the input attachments, add a dependency between the passes
                auto is_color_producer = false;
                auto is_depth_producer = false;
                auto it = input_attachments_unproduced.begin();
                while(it != input_attachments_unproduced.end()) {
                    if(std::find(
                        previous_subpass.color_attachments.begin(),
                        previous_subpass.color_attachments.end(),
                        *it
                    ) != previous_subpass.color_attachments.end()) {
                        it = input_attachments_unproduced.erase(it);
                        is_color_producer = true;
                    } else if(previous_subpass.depth_attachment && *previous_subpass.depth_attachment == *it) {
                        it = input_attachments_unproduced.erase(it);
                        is_depth_producer = true;
                    } else {
                        ++it;
                    }
                }

                if(is_color_producer || is_depth_producer) {
                    dependencies.emplace_back(
                        VkSubpassDependency2{
                            .sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
                            .srcSubpass = static_cast<uint32_t>(producer_index),
                            .dstSubpass = subpass_index,
                            .srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                        }
                    );
                }

                // Early-out
                if(input_attachments_unproduced.empty()) {
                    break;
                }
            }
        }
    }

    auto create_info = VkRenderPassCreateInfo2{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .subpassCount = static_cast<uint32_t>(subpasses.size()),
        .pSubpasses = subpasses.data(),
        .dependencyCount = static_cast<uint32_t>(dependencies.size()),
        .pDependencies = dependencies.data(),
    };

    logger->debug("Creating a render pass with {} subpasses", create_info.subpassCount);
    for(auto subpass_index = 0u; subpass_index < create_info.subpassCount; subpass_index++) {
        const auto& subpass = create_info.pSubpasses[subpass_index];
        logger->debug("Subpass {} has {} color attachments", subpass_index, subpass.colorAttachmentCount);
        for(auto attachment_index = 0u; attachment_index < subpass.colorAttachmentCount; attachment_index++) {
            const auto& attachment_ref = subpass.pColorAttachments[attachment_index];
            logger->debug(
                "\tattachment={} layout={}",
                attachment_ref.attachment,
                string_VkImageLayout(attachment_ref.layout)
            );
        }
        logger->debug(
            "Subpass {} has {} depth attachments",
            subpass_index,
            subpass.pDepthStencilAttachment == nullptr ? 0 : 1
        );
        if(subpass.pDepthStencilAttachment != nullptr) {
            logger->debug(
                "\tattachment={} layout={}",
                subpass.pDepthStencilAttachment->attachment,
                string_VkImageLayout(subpass.pDepthStencilAttachment->layout)
            );
        }
        logger->debug("Subpass {} has {} input attachments", subpass_index, subpass.inputAttachmentCount);
        for(auto attachment_index = 0u; attachment_index < subpass.inputAttachmentCount; attachment_index++) {
            const auto& attachment_ref = subpass.pInputAttachments[attachment_index];
            logger->debug(
                "\tattachment={} layout={}",
                attachment_ref.attachment,
                string_VkImageLayout(attachment_ref.layout)
            );
        }
    }
    if(create_info.dependencyCount > 0) {
        logger->debug("Dependencies:");
        for(auto dependency_index = 0u; dependency_index < create_info.dependencyCount; dependency_index++) {
            const auto& dependency = create_info.pDependencies[dependency_index];
            logger->debug("\tDependency between subpass {} and {}", dependency.srcSubpass, dependency.dstSubpass);
            logger->debug("\t\tsrcStageMask={:x}, dstStageMask={:x}", dependency.srcStageMask, dependency.dstStageMask);
            logger->debug(
                "\t\tsrcAccessMask={:x}, dstAccessMask={:x}",
                dependency.srcAccessMask,
                dependency.dstAccessMask
            );
        }
    }

    VkRenderPass render_pass;
    {
        ZoneScopedN("vkCreateRenderPass");
        vkCreateRenderPass2(backend.get_device().device, &create_info, nullptr, &render_pass);

        backend.set_object_name(render_pass, pass.name);
    }

    cached_render_passes.emplace(pass.name, render_pass);

    return render_pass;
}

void ResourceAllocator::free_resources_for_frame(const uint32_t frame_idx) {
    ZoneScoped;

    const auto device = backend.get_device().device;

    auto& zombie_ases = as_zombie_lists[frame_idx];
    for(const auto& as : zombie_ases) {
        vkDestroyAccelerationStructureKHR(device, as->acceleration_structure, nullptr);

        std::erase(acceleration_structures, *as);
    }
    zombie_ases.clear();

    auto& zombie_buffers = buffer_zombie_lists[frame_idx];
    for(auto handle : zombie_buffers) {
        vmaDestroyBuffer(vma, handle->buffer, handle->allocation);

        std::erase(buffers, *handle);
    }

    zombie_buffers.clear();

    auto& zombie_textures = texture_zombie_lists[frame_idx];
    for(auto handle : zombie_textures) {
        vkDestroyImageView(device, handle->image_view, nullptr);

        switch(handle->type) {
        case TextureAllocationType::Vma:
            vmaDestroyImage(vma, handle->image, handle->vma.allocation);
            break;

        case TextureAllocationType::Ktx:
            ktxVulkanTexture_Destruct(&handle->ktx.ktx_vk_tex, device, nullptr);
            break;

        case TextureAllocationType::Swapchain:
            // We just need to destroy the image view
            vkDestroyImageView(device, handle->image_view, nullptr);
            break;

        default:
            throw std::runtime_error{"Unknown texture allocation type"};
        }

        std::erase(textures, *handle);
    }

    zombie_textures.clear();

    auto& zombie_framebuffers = framebuffer_zombie_lists[frame_idx];
    for(const auto& framebuffer : zombie_framebuffers) {
        vkDestroyFramebuffer(device, framebuffer.framebuffer, nullptr);
    }
    zombie_framebuffers.clear();

    vmaSetCurrentFrameIndex(vma, frame_idx);
}

void ResourceAllocator::report_memory_usage() const {
    auto budgets = std::vector<VmaBudget>{};
    budgets.resize(32);
    vmaGetHeapBudgets(vma, budgets.data());

    /*
     * Google Pixel 7:
     * Memory usage report
     * Index | Block Count | Allocation Count | Block Bytes | Allocation Bytes | Total
     *     0 |          23 |               50 |   302570368 |        212183360 | 302570368 out of 6229288550
     *     1 |           0 |                0 |           0 |                0 | 0 out of 83886080
     *
     * RTX 2080 Super:
     * Memory usage report
     * Index | Block Count | Allocation Count | Block Bytes | Allocation Bytes | Total
     *     0 |          24 |               43 |   251064576 |        191568192 | 251064576 out of 6711725260
     *     1 |           1 |                4 |    33554432 |           215040 | 33554432 out of 27450340147
     *     2 |           1 |                3 |     3506176 |             1984 | 3506176 out of 179516211
     */

    logger->info("Memory usage report");
    logger->info("Index | Block Count | Allocation Count | Block Bytes | Allocation Bytes | Total");

    auto budget_index = 0;
    for(const auto& budget : budgets) {
        if(budget.budget == 0) {
            continue;
        }

        logger->info(
            "{:>5} | {:>11} | {:>16} | {:>11} | {:>16} | {} out of {}",
            budget_index,
            budget.statistics.blockCount,
            budget.statistics.allocationCount,
            budget.statistics.blockBytes,
            budget.statistics.allocationBytes,
            budget.usage,
            budget.budget
        );

        budget_index++;
    }
}

VmaAllocator ResourceAllocator::get_vma() const {
    return vma;
}
