#include "graphics_pipeline.hpp"

#include <magic_enum.hpp>
#include <spirv_reflect.h>
#include <spdlog/logger.h>
#include <tracy/Tracy.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/android_sink.h>

#include "render/backend/render_backend.hpp"
#include "core/system_interface.hpp"

static std::shared_ptr<spdlog::logger> logger;

void GraphicsPipeline::create_pipeline_layout(
    RenderBackend& backend,
    const absl::flat_hash_map<uint32_t, DescriptorSetInfo>& descriptor_set_infos, 
    const std::vector<VkPushConstantRange>& push_constants
) {
    // Create descriptor sets
    descriptor_set_layouts.reserve(descriptor_set_infos.size());

    auto& cache = backend.get_descriptor_cache();

    for (const auto& [set_index, set_info] : descriptor_set_infos) {
        if (descriptor_set_layouts.size() <= set_index) {
            descriptor_set_layouts.resize(set_index + 1);
        }

        auto bindings = std::vector<VkDescriptorSetLayoutBinding>{};

        for (const auto& binding : set_info.bindings) {
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
            bindings.back().stageFlags = VK_SHADER_STAGE_ALL;
        }
                
        const auto layout = cache.create_descriptor_layout(&create_info);

        descriptor_set_layouts[set_index] = layout;
    }
    
    const auto create_info = VkPipelineLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts.size()),
        .pSetLayouts = descriptor_set_layouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(push_constants.size()),
        .pPushConstantRanges = push_constants.data(),
    };

    vkCreatePipelineLayout(backend.get_device(), &create_info, nullptr, &pipeline_layout);

    if(!pipeline_name.empty()) {
        backend.set_object_name(pipeline_layout, pipeline_name);
    }
}

VkPipelineLayout GraphicsPipeline::get_layout() const {
    return pipeline_layout;
}

uint32_t GraphicsPipeline::get_num_push_constants() const { return num_push_constants; }
VkShaderStageFlags GraphicsPipeline::get_push_constant_shader_stages() const { return push_constant_stages; }

const DescriptorSetInfo& GraphicsPipeline::get_descriptor_set_info(const uint32_t set_index) const {
    return descriptor_sets.at(set_index);
}
