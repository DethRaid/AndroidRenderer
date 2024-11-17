#include "descriptor_set_builder.hpp"

#include <spdlog/fmt/bundled/format.h>

#include "render/backend/descriptor_set_allocator.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/utils.hpp"

static bool is_buffer_type(VkDescriptorType vk_type);

static bool is_texture_type(VkDescriptorType vk_type);

static bool is_combined_image_sampler(VkDescriptorType vk_type);

static bool is_acceleration_structure(VkDescriptorType vk_type);

static VkImageLayout to_image_layout(VkDescriptorType descriptor_type, bool is_depth);

DescriptorSet::DescriptorSet(
    RenderBackend& backend_in, DescriptorSetAllocator& allocator_in, DescriptorSetInfo set_info_in
) : backend{backend_in}, allocator{allocator_in}, set_info{std::move(set_info_in)} {}

DescriptorSet& DescriptorSet::bind(const uint32_t binding_index, const BufferHandle buffer) {
#ifndef _NDEBUG
    auto itr = set_info.bindings.find(binding_index);
    if (itr == set_info.bindings.end()) {
        throw std::runtime_error{
            fmt::format(
                "Tried to bind a resource to binding {}, but that does not exist in this descriptor set", binding_index
            )
        };
    }

    if (!is_buffer_type(itr->second.descriptorType)) {
        throw std::runtime_error{fmt::format("Binding {} is not a buffer binding!", binding_index)};
    }
#endif

    bindings.emplace(binding_index, BoundResource{ .buffer = buffer });

    return *this;
}

DescriptorSet& DescriptorSet::bind(const uint32_t binding_index, const TextureHandle texture) {
#ifndef _NDEBUG
    auto itr = set_info.bindings.find(binding_index);
    if (itr == set_info.bindings.end()) {
        throw std::runtime_error{
            fmt::format(
                "Tried to bind a resource to binding {}, but that does not exist in this descriptor set", binding_index
            )
        };
    }

    if (!is_texture_type(itr->second.descriptorType)) {
        throw std::runtime_error{fmt::format("Binding {} is not a texture binding!", binding_index)};
    }
#endif

    bindings[binding_index].texture = texture;

    return *this;
}

DescriptorSet& DescriptorSet::bind(uint32_t binding_index, TextureHandle texture, VkSampler vk_sampler) {
#ifndef _NDEBUG
    auto itr = set_info.bindings.find(binding_index);
    if (itr == set_info.bindings.end()) {
        throw std::runtime_error{
            fmt::format(
                "Tried to bind a resource to binding {}, but that does not exist in this descriptor set", binding_index
            )
        };
    }

    if (!is_combined_image_sampler(itr->second.descriptorType)) {
        throw std::runtime_error{ fmt::format("Binding {} is not a combined image/sampler binding!", binding_index) };
    }
#endif

    bindings[binding_index].combined_image_sampler = { texture, vk_sampler };

    return *this;
    
}

DescriptorSet& DescriptorSet::bind(
    const uint32_t binding_index, const AccelerationStructureHandle acceleration_structure
) {
#ifndef _NDEBUG
    auto itr = set_info.bindings.find(binding_index);
    if (itr == set_info.bindings.end()) {
        throw std::runtime_error{
            fmt::format(
                "Tried to bind a resource to binding {}, but that does not exist in this descriptor set", binding_index
            )
        };
    }

    if (!is_acceleration_structure(itr->second.descriptorType)) {
        throw std::runtime_error{fmt::format("Binding {} is not a acceleration structure binding!", binding_index)};
    }
#endif

    bindings[binding_index].address = acceleration_structure->as_address;

    return *this;
}

DescriptorSet& DescriptorSet::finalize() {
    auto builder = vkutil::DescriptorBuilder::begin(backend, allocator);
    auto& resources = backend.get_global_allocator();
    for (const auto& [binding, resource] : bindings) {
        const auto& binding_info = set_info.bindings.at(binding);
        if (is_buffer_type(binding_info.descriptorType)) {
            builder.bind_buffer(
                binding, {.buffer = resource.buffer}, binding_info.descriptorType, binding_info.stageFlags
            );

        } else if (is_texture_type(binding_info.descriptorType)) {
            const auto& texture_actual = resources.get_texture(resource.texture);
            builder.bind_image(
                binding, {
                    .image = resource.texture,
                    .image_layout = to_image_layout(
                        binding_info.descriptorType, is_depth_format(texture_actual.create_info.format)
                    )
                }, binding_info.descriptorType, binding_info.stageFlags
            );

        } else if(is_combined_image_sampler(binding_info.descriptorType)) {
            const auto& texture_actual = resources.get_texture(resource.combined_image_sampler.texture);
            builder.bind_image(
                binding, {
                    .sampler = resource.combined_image_sampler.sampler,
                    .image = resource.texture,
                    .image_layout = to_image_layout(
                        binding_info.descriptorType, is_depth_format(texture_actual.create_info.format)
                    )
                }, binding_info.descriptorType, binding_info.stageFlags
            );

            
        } else {
            throw std::runtime_error{ "Unknown descriptor type!" };
        }
    }

    descriptor_set = *builder.build();

    return *this;
}

VkAccessFlags2 to_vk_access(const VkDescriptorType& descriptor_type, const bool is_read_only) {
    switch (descriptor_type) {
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        [[fallthrough]];
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        [[fallthrough]];
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        return VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;

    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        [[fallthrough]];
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        [[fallthrough]];
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        [[fallthrough]];
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        if (is_read_only) {
            return VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
        } else {
            return VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
        }

    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        [[fallthrough]];
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        return VK_ACCESS_2_UNIFORM_READ_BIT;

    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT;

    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        [[fallthrough]];
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV:
        return VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    }

    return VK_ACCESS_2_NONE;
}

VkPipelineStageFlags2 to_pipeline_stage(const VkShaderStageFlags stage_flags) {
    VkPipelineStageFlags2 flags = 0;

    if (stage_flags & VK_SHADER_STAGE_VERTEX_BIT) {
        flags |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    }
    if (stage_flags & VK_SHADER_STAGE_GEOMETRY_BIT) {
        flags |= VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT;
    }
    if (stage_flags & VK_SHADER_STAGE_FRAGMENT_BIT) {
        flags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    }
    if (stage_flags & VK_SHADER_STAGE_COMPUTE_BIT) {
        flags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    }
    if (stage_flags & VK_SHADER_STAGE_RAYGEN_BIT_KHR) {
        flags |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    }
    if (stage_flags & VK_SHADER_STAGE_ANY_HIT_BIT_KHR) {
        flags |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    }
    if (stage_flags & VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) {
        flags |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    }
    if (stage_flags & VK_SHADER_STAGE_MISS_BIT_KHR) {
        flags |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    }
    if (stage_flags & VK_SHADER_STAGE_INTERSECTION_BIT_KHR) {
        flags |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    }
    if (stage_flags & VK_SHADER_STAGE_CALLABLE_BIT_KHR) {
        flags |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    }
    if (stage_flags & VK_SHADER_STAGE_TASK_BIT_EXT) {
        flags |= VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT;
    }
    if (stage_flags & VK_SHADER_STAGE_MESH_BIT_EXT) {
        flags |= VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT;
    }

    return flags;
}

VkImageLayout to_image_layout(const VkDescriptorType descriptor_type, const bool is_depth) {
    switch (descriptor_type) {
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        [[fallthrough]];
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return VK_IMAGE_LAYOUT_GENERAL;

    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        if (is_depth) {
            return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
        } else {
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
    }

    return VK_IMAGE_LAYOUT_UNDEFINED;
}

void DescriptorSet::get_resource_usage_information(
    TextureUsageMap& texture_usages, BufferUsageMap& buffer_usages
) const {
    for (const auto& [binding, resource] : bindings) {
        const auto& binding_info = set_info.bindings.at(binding);
        if (is_buffer_type(binding_info.descriptorType)) {
            const auto& buffer_handle = resource.buffer;
            if (auto itr = buffer_usages.find(buffer_handle); itr != buffer_usages.end()) {
                itr->second.access |= to_vk_access(binding_info.descriptorType, binding_info.is_read_only);
                itr->second.stage |= to_pipeline_stage(binding_info.stageFlags);
            } else {
                buffer_usages[buffer_handle] = {
                    .stage = to_pipeline_stage(binding_info.stageFlags),
                    .access = to_vk_access(binding_info.descriptorType, binding_info.is_read_only)
                };
            }
        } else if (is_texture_type(binding_info.descriptorType)) {
            const auto& texture_handle = resource.texture;
            if (auto itr = texture_usages.find(texture_handle); itr != texture_usages.end()) {
                itr->second.access |= to_vk_access(binding_info.descriptorType, binding_info.is_read_only);
                itr->second.stage |= to_pipeline_stage(binding_info.stageFlags);
            } else {
                const auto& allocator = backend.get_global_allocator();
                const auto& texture_actual = allocator.get_texture(texture_handle);
                const auto is_depth = is_depth_format(texture_actual.create_info.format);
                texture_usages[texture_handle] = {
                    .stage = to_pipeline_stage(binding_info.stageFlags),
                    .access = to_vk_access(binding_info.descriptorType, binding_info.is_read_only),
                    .layout = to_image_layout(binding_info.descriptorType, is_depth)
                };
            }
        } else if(is_combined_image_sampler(binding_info.descriptorType)) {
            const auto& texture_handle = resource.combined_image_sampler.texture;
            if (auto itr = texture_usages.find(texture_handle); itr != texture_usages.end()) {
                itr->second.access |= to_vk_access(binding_info.descriptorType, binding_info.is_read_only);
                itr->second.stage |= to_pipeline_stage(binding_info.stageFlags);
            } else {
                const auto& allocator = backend.get_global_allocator();
                const auto& texture_actual = allocator.get_texture(texture_handle);
                const auto is_depth = is_depth_format(texture_actual.create_info.format);
                texture_usages[texture_handle] = {
                    .stage = to_pipeline_stage(binding_info.stageFlags),
                    .access = to_vk_access(binding_info.descriptorType, binding_info.is_read_only),
                    .layout = to_image_layout(binding_info.descriptorType, is_depth)
                };
            }
        }
    }
}

VkDescriptorSet DescriptorSet::get_vk_descriptor_set() const { return descriptor_set; }

bool is_buffer_type(const VkDescriptorType vk_type) {
    return
        vk_type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
        vk_type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
        vk_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
        vk_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
        vk_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
        vk_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
}

bool is_texture_type(const VkDescriptorType vk_type) {
    return
        vk_type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
        vk_type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
        vk_type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
}
bool is_combined_image_sampler(const VkDescriptorType vk_type) {
    return vk_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
}

bool is_acceleration_structure(const VkDescriptorType vk_type) {
    return
        vk_type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
}
