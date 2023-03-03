#include "compute_shader.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/android_sink.h>

#include "core/system_interface.hpp"
#include "render/backend/pipeline.hpp"

static std::shared_ptr<spdlog::logger> logger;

tl::optional<ComputeShader>
ComputeShader::create(VkDevice device, const std::string& name, const std::vector<uint8_t>& instructions) {
    if (logger == nullptr) {
        logger = SystemInterface::get().get_logger("ComputeShader");
    }

    const auto module_create_info = VkShaderModuleCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = instructions.size(),
            .pCode = reinterpret_cast<const uint32_t*>(instructions.data()),
    };
    VkShaderModule module;
    auto result = vkCreateShaderModule(device, &module_create_info, nullptr, &module);
    if (result != VK_SUCCESS) {
        logger->error("Could not create compute shader {}: Vulkan error {}", name, result);
        return tl::nullopt;
    }

    std::unordered_map<uint32_t, DescriptorSetInfo> descriptor_sets;
    std::vector<VkPushConstantRange> push_constants;
    collect_bindings(instructions, name, VK_SHADER_STAGE_COMPUTE_BIT, descriptor_sets, push_constants);

    auto layouts = std::vector<VkDescriptorSetLayout>{};
    layouts.reserve(descriptor_sets.size());

    for (const auto&[set_index, info]: descriptor_sets) {
        auto bindings = std::vector<VkDescriptorSetLayoutBinding>{};
        bindings.resize(info.size());

        for (const auto&[binding_index, binding]: info) {
            bindings[binding_index] = binding;
        }

        auto create_info = VkDescriptorSetLayoutCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = static_cast<uint32_t>(bindings.size()),
                .pBindings = bindings.data(),
        };
        auto dsl = VkDescriptorSetLayout{};
        result = vkCreateDescriptorSetLayout(device, &create_info, nullptr, &dsl);
        if (result != VK_SUCCESS) {
            logger->error("Could not create descriptor set layout {} for shader {}: Vulkan error {}", set_index, name,
                          result);
            return tl::nullopt;
        }

        layouts.push_back(dsl);
    }

    const auto pipeline_layout_create_info = VkPipelineLayoutCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data(),
            .pushConstantRangeCount = static_cast<uint32_t>(push_constants.size()),
            .pPushConstantRanges = push_constants.data(),
    };
    auto pipeline_layout = VkPipelineLayout{};
    result = vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &pipeline_layout);
    if (result != VK_SUCCESS) {
        vkDestroyShaderModule(device, module, nullptr);
        for (const auto layout: layouts) {
            vkDestroyDescriptorSetLayout(device, layout, nullptr);
        }

        logger->error("Could not create pipeline layout for shader {}: Vulkan error {}", name, result);
        return tl::nullopt;
    }

    const auto create_info = VkComputePipelineCreateInfo{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = VkPipelineShaderStageCreateInfo{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                    .module = module,
                    .pName = "main",
            },
            .layout = pipeline_layout
    };
    auto pipeline = VkPipeline{};
    vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline);
    result = vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &pipeline_layout);
    if (result != VK_SUCCESS) {
        vkDestroyShaderModule(device, module, nullptr);
        for (const auto layout: layouts) {
            vkDestroyDescriptorSetLayout(device, layout, nullptr);
        }

        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);

        logger->error("Could not create pipeline {}: Vulkan error {}", name, result);
        return tl::nullopt;
    }

    vkDestroyShaderModule(device, module, nullptr);
    for (const auto layout: layouts) {
        vkDestroyDescriptorSetLayout(device, layout, nullptr);
    }

    const auto layout_name = fmt::format("{} Layout", name);
    if(vkSetDebugUtilsObjectNameEXT != nullptr) {
        const auto layout_name_info = VkDebugUtilsObjectNameInfoEXT{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = VK_OBJECT_TYPE_PIPELINE_LAYOUT,
            .objectHandle = reinterpret_cast<uint64_t>(pipeline_layout),
            .pObjectName = layout_name.c_str()
        };
        vkSetDebugUtilsObjectNameEXT(device, &layout_name_info);

        const auto pipeline_name_info = VkDebugUtilsObjectNameInfoEXT{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = VK_OBJECT_TYPE_PIPELINE,
            .objectHandle = reinterpret_cast<uint64_t>(pipeline),
            .pObjectName = name.c_str()
        };
        vkSetDebugUtilsObjectNameEXT(device, &pipeline_name_info);
    }

    return ComputeShader{.layout = pipeline_layout, .pipeline = pipeline};
}
