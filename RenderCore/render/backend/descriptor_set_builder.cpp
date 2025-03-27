#include "descriptor_set_builder.hpp"

#include <spdlog/fmt/bundled/format.h>

#include "render/backend/descriptor_set_allocator.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/utils.hpp"


static VkAccessFlags2 to_vk_access(const VkDescriptorType& descriptor_type, bool is_read_only);

static VkPipelineStageFlags2 to_pipeline_stage(VkShaderStageFlags stage_flags);

static VkImageLayout to_image_layout(VkDescriptorType descriptor_type);

static bool is_buffer_type(VkDescriptorType vk_type);

static bool is_texture_type(VkDescriptorType vk_type);

static bool is_combined_image_sampler(VkDescriptorType vk_type);

static bool is_acceleration_structure(VkDescriptorType vk_type);

void DescriptorSet::get_resource_usage_information(
    eastl::vector<TextureUsageToken>& texture_usages, eastl::vector<BufferUsageToken>& buffer_usages
) const {
    auto binding_idx = 0;
    for(const auto& resource : bindings) {
        const auto& binding_info = set_info.bindings.at(binding_idx);
        if(is_buffer_type(binding_info.descriptorType)) {
            const auto& buffer_handle = resource.buffer;
            if(const auto itr = std::ranges::find_if(
                buffer_usages,
                [=](const auto& usage) {
                    return usage.buffer == buffer_handle;
                }); itr != buffer_usages.end()) {
                itr->access |= to_vk_access(binding_info.descriptorType, binding_info.is_read_only);
                itr->stage |= to_pipeline_stage(binding_info.stageFlags);

            } else {
                buffer_usages.emplace_back(
                    BufferUsageToken{
                        .buffer = buffer_handle,
                        .stage = to_pipeline_stage(binding_info.stageFlags),
                        .access = to_vk_access(binding_info.descriptorType, binding_info.is_read_only)
                    });
            }

        } else if(is_texture_type(binding_info.descriptorType)) {
            const auto& texture_handle = resource.texture;
            if(auto itr = std::ranges::find_if(
                texture_usages,
                [=](const auto& usage) {
                    return usage.texture == texture_handle;
                }); itr != texture_usages.end()) {
                itr->access |= to_vk_access(binding_info.descriptorType, binding_info.is_read_only);
                itr->stage |= to_pipeline_stage(binding_info.stageFlags);

            } else {
                texture_usages.emplace_back(
                    TextureUsageToken{
                        .texture = texture_handle,
                        .stage = to_pipeline_stage(binding_info.stageFlags),
                        .access = to_vk_access(binding_info.descriptorType, binding_info.is_read_only),
                        .layout = to_image_layout(binding_info.descriptorType)
                    });
            }

        } else if(is_combined_image_sampler(binding_info.descriptorType)) {
            const auto& texture_handle = resource.combined_image_sampler.texture;
            if(auto itr = std::ranges::find_if(
                texture_usages,
                [=](const auto& usage) {
                    return usage.texture == texture_handle;
                }); itr != texture_usages.end()) {
                itr->access |= to_vk_access(binding_info.descriptorType, binding_info.is_read_only);
                itr->stage |= to_pipeline_stage(binding_info.stageFlags);

            } else {
                texture_usages.emplace_back(
                    TextureUsageToken{
                        .texture = texture_handle,
                        .stage = to_pipeline_stage(binding_info.stageFlags),
                        .access = to_vk_access(binding_info.descriptorType, binding_info.is_read_only),
                        .layout = to_image_layout(binding_info.descriptorType)
                    });
            }

            // Acceleration structures are just spicy buffers
        } else if(is_acceleration_structure(binding_info.descriptorType)) {
            const auto& as_handle = resource.acceleration_structure;
            if(auto itr = std::ranges::find_if(
                buffer_usages,
                [=](const auto& usage) {
                    return usage.buffer == as_handle->buffer;
                }); itr != buffer_usages.end()) {
                itr->access |= to_vk_access(binding_info.descriptorType, binding_info.is_read_only);
                itr->stage |= to_pipeline_stage(binding_info.stageFlags);

            } else {
                buffer_usages.emplace_back(
                    BufferUsageToken{
                        .buffer = as_handle->buffer,
                        .stage = to_pipeline_stage(binding_info.stageFlags),
                        .access = to_vk_access(binding_info.descriptorType, binding_info.is_read_only),
                    });
            }
        }

        binding_idx++;
    }
}

DescriptorSetBuilder::DescriptorSetBuilder(
    RenderBackend& backend_in, DescriptorSetAllocator& allocator_in, DescriptorSetInfo set_info_in,
    const std::string_view name_in
) : backend{&backend_in}, allocator{&allocator_in}, set_info{std::move(set_info_in)}, name{name_in} {
    bindings.resize(set_info.bindings.size());
}

DescriptorSetBuilder& DescriptorSetBuilder::bind(const BufferHandle buffer) {
#ifndef _NDEBUG
    if(binding_index >= set_info.bindings.size()) {
        throw std::runtime_error{
            fmt::format(
                "Tried to bind a resource to binding {}, but that does not exist in this descriptor set",
                binding_index
            )
        };
    }

    if(!is_buffer_type(set_info.bindings[binding_index].descriptorType)) {
        throw std::runtime_error{fmt::format("Binding {} is not a buffer binding!", binding_index)};
    }
#endif

    bindings[binding_index] = detail::BoundResource{.buffer = buffer};

    binding_index++;

    return *this;
}

DescriptorSetBuilder& DescriptorSetBuilder::bind(const TextureHandle texture) {
#ifndef _NDEBUG
    if(binding_index >= set_info.bindings.size()) {
        throw std::runtime_error{
            fmt::format(
                "Tried to bind a resource to binding {}, but that does not exist in this descriptor set",
                binding_index
            )
        };
    }

    if(!is_texture_type(set_info.bindings[binding_index].descriptorType)) {
        throw std::runtime_error{fmt::format("Binding {} is not a texture binding!", binding_index)};
    }
#endif

    bindings[binding_index].texture = texture;

    binding_index++;

    return *this;
}

DescriptorSetBuilder& DescriptorSetBuilder::bind(const TextureHandle texture, const VkSampler vk_sampler) {
#ifndef _NDEBUG
    if(binding_index >= set_info.bindings.size()) {
        throw std::runtime_error{
            fmt::format(
                "Tried to bind a resource to binding {}, but that does not exist in this descriptor set",
                binding_index
            )
        };
    }

    if(!is_combined_image_sampler(set_info.bindings[binding_index].descriptorType)) {
        throw std::runtime_error{fmt::format("Binding {} is not a combined image/sampler binding!", binding_index)};
    }
#endif

    bindings[binding_index].combined_image_sampler = {texture, vk_sampler};

    binding_index++;

    return *this;
}

DescriptorSetBuilder& DescriptorSetBuilder::bind(const AccelerationStructureHandle acceleration_structure) {
#ifndef _NDEBUG
    if(binding_index >= set_info.bindings.size()) {
        throw std::runtime_error{
            fmt::format(
                "Tried to bind a resource to binding {}, but that does not exist in this descriptor set",
                binding_index
            )
        };
    }

    if(!is_acceleration_structure(set_info.bindings[binding_index].descriptorType)) {
        throw std::runtime_error{fmt::format("Binding {} is not a acceleration structure binding!", binding_index)};
    }
#endif

    bindings[binding_index].acceleration_structure = acceleration_structure;

    binding_index++;

    return *this;
}

DescriptorSetBuilder& DescriptorSetBuilder::next_binding(const uint32_t binding_index) {
    this->binding_index = binding_index;

    return *this;
}

DescriptorSet DescriptorSetBuilder::build() {
    ZoneScoped;

    auto builder = vkutil::DescriptorBuilder::begin(*backend, *allocator);
    auto binding_idx = 0u;
    for(const auto& resource : bindings) {
        const auto& binding_info = set_info.bindings.at(binding_idx);
        if(is_buffer_type(binding_info.descriptorType)) {
            builder.bind_buffer(
                binding_idx,
                {.buffer = resource.buffer},
                binding_info.descriptorType,
                binding_info.stageFlags
            );

        } else if(is_texture_type(binding_info.descriptorType)) {
            builder.bind_image(
                binding_idx,
                {
                    .image = resource.texture,
                    .image_layout = to_image_layout(binding_info.descriptorType)
                },
                binding_info.descriptorType,
                binding_info.stageFlags
            );

        } else if(is_combined_image_sampler(binding_info.descriptorType)) {
            builder.bind_image(
                binding_idx,
                {
                    .sampler = resource.combined_image_sampler.sampler,
                    .image = resource.texture,
                    .image_layout = to_image_layout(binding_info.descriptorType)
                },
                binding_info.descriptorType,
                binding_info.stageFlags
            );

        } else if(is_acceleration_structure(binding_info.descriptorType)) {
            builder.bind_acceleration_structure(
                binding_idx,
                {.as = resource.acceleration_structure},
                binding_info.stageFlags);

        }

        binding_idx++;
    }

    VkDescriptorSetLayout layout;
    const auto descriptor_set = *builder.build(layout);

    backend->set_object_name(descriptor_set, name);

    return DescriptorSet{
        descriptor_set, layout, std::move(set_info), std::move(bindings)
    };
}

VkAccessFlags2 to_vk_access(const VkDescriptorType& descriptor_type, const bool is_read_only) {
    switch(descriptor_type) {
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
        if(is_read_only) {
            return VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
        } else {
            return VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
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

    if(stage_flags & VK_SHADER_STAGE_VERTEX_BIT) {
        flags |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    }
    if(stage_flags & VK_SHADER_STAGE_GEOMETRY_BIT) {
        flags |= VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT;
    }
    if(stage_flags & VK_SHADER_STAGE_FRAGMENT_BIT) {
        flags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    }
    if(stage_flags & VK_SHADER_STAGE_COMPUTE_BIT) {
        flags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    }
    if(stage_flags & VK_SHADER_STAGE_RAYGEN_BIT_KHR) {
        flags |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    }
    if(stage_flags & VK_SHADER_STAGE_ANY_HIT_BIT_KHR) {
        flags |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    }
    if(stage_flags & VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) {
        flags |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    }
    if(stage_flags & VK_SHADER_STAGE_MISS_BIT_KHR) {
        flags |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    }
    if(stage_flags & VK_SHADER_STAGE_INTERSECTION_BIT_KHR) {
        flags |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    }
    if(stage_flags & VK_SHADER_STAGE_CALLABLE_BIT_KHR) {
        flags |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    }
    if(stage_flags & VK_SHADER_STAGE_TASK_BIT_EXT) {
        flags |= VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT;
    }
    if(stage_flags & VK_SHADER_STAGE_MESH_BIT_EXT) {
        flags |= VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT;
    }

    return flags;
}

VkImageLayout to_image_layout(const VkDescriptorType descriptor_type) {
    switch(descriptor_type) {
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        [[fallthrough]];
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return VK_IMAGE_LAYOUT_GENERAL;

    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        throw std::runtime_error{"Input attachments are not supported"};
    }

    return VK_IMAGE_LAYOUT_UNDEFINED;
}

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
        vk_type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR ||
        vk_type == VK_DESCRIPTOR_TYPE_PARTITIONED_ACCELERATION_STRUCTURE_NV;
}
