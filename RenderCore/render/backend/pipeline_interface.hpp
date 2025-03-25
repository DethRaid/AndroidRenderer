#pragma once

#include <string>

#include <volk.h>

#include "render/backend/descriptor_set_info.hpp"

class RenderBackend;

struct PipelineBase {
    std::string name;

    VkPipeline pipeline = VK_NULL_HANDLE;

    VkPipelineLayout layout = VK_NULL_HANDLE;

    uint32_t num_push_constants = 0;

    VkShaderStageFlags push_constant_stages = 0;

    eastl::vector<DescriptorSetInfo> descriptor_sets;

    eastl::vector<VkDescriptorSetLayout> descriptor_set_layouts;

    PipelineBase() = default;

    ~PipelineBase();

    explicit PipelineBase(const PipelineBase& other) = delete;

    PipelineBase& operator=(const PipelineBase& other) = delete;

    PipelineBase(PipelineBase&& old) noexcept;

    PipelineBase& operator=(PipelineBase&& old) noexcept;

    void create_pipeline_layout(
        RenderBackend& backend, const eastl::vector<DescriptorSetInfo>& descriptor_set_infos,
        const eastl::vector<VkPushConstantRange>& push_constants
    );
};
