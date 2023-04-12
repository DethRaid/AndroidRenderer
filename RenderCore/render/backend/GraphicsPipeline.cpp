#include "graphics_pipeline.hpp"

#include <spirv_reflect.h>
#include <spdlog/logger.h>
#include <tracy/Tracy.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/android_sink.h>

#include "render/backend/render_backend.hpp"
#include "magic_enum.hpp"
#include "core/system_interface.hpp"

static std::shared_ptr<spdlog::logger> logger;

void GraphicsPipeline::create_pipeline_layout(
    RenderBackend& backend,
    const std::unordered_map<uint32_t, DescriptorSetInfo>& descriptor_set_infos
) {
    // Create descriptor sets
    auto descriptor_set_layouts = std::vector<VkDescriptorSetLayout>{};
    descriptor_set_layouts.reserve(descriptor_set_infos.size());

    for (const auto& [set_index, set_info] : descriptor_set_infos) {
        if (descriptor_set_layouts.size() <= set_index) {
            descriptor_set_layouts.resize(set_index + 1);
        }

        auto bindings = std::vector<VkDescriptorSetLayoutBinding>{};

        for (const auto& [index, binding] : set_info.bindings) {
            bindings.emplace_back(binding);
        }

        auto create_info = VkDescriptorSetLayoutCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data(),
        };

        // If the last binding is un unsized texture array, tell Vulkan about it
        auto flags_create_info = VkDescriptorSetLayoutBindingFlagsCreateInfo{};
        auto flags = std::vector<VkDescriptorBindingFlags>{};
        if (set_info.has_variable_count_binding) {
            flags.resize(bindings.size());
            flags.back() = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
            flags_create_info = VkDescriptorSetLayoutBindingFlagsCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount = static_cast<uint32_t>(flags.size()),
                .pBindingFlags = flags.data(),
            };
            create_info.pNext = &flags_create_info;
        }

        auto layout = VkDescriptorSetLayout{};
        vkCreateDescriptorSetLayout(backend.get_device(), &create_info, nullptr, &layout);
        descriptor_set_layouts[set_index] = layout;
    }

    const auto push_constants = VkPushConstantRange{
        .stageFlags = VK_SHADER_STAGE_ALL,
        .offset = 0,
        .size = 8 * sizeof(uint32_t)
    };

    const auto create_info = VkPipelineLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts.size()),
        .pSetLayouts = descriptor_set_layouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constants,
    };

    vkCreatePipelineLayout(backend.get_device(), &create_info, nullptr, &pipeline_layout);

    if(!pipeline_name.empty()) {
        backend.set_object_name(pipeline_layout, pipeline_name);
    }
}

VkPipelineLayout GraphicsPipeline::get_layout() const {
    return pipeline_layout;
}

VkDescriptorType to_vk_type(SpvReflectDescriptorType type) {
    switch (type) {
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
        return VK_DESCRIPTOR_TYPE_SAMPLER;

    case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;

    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;

    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;

    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;

    case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;

    case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    default:
        spdlog::error("Unknown descriptor type {}", type);
        return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
}
