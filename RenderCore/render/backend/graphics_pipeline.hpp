#pragma once

#include <filesystem>
#include <vector>
#include <cstdint>
#include <unordered_map>

#include <volk.h>
#include <tl/optional.hpp>

#include "core/object_pool.hpp"

class RenderBackend;

struct DescriptorSetInfo {
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;

    bool has_variable_count_binding = false;
};

/**
 * Simple pipeline abstraction
 *
 * Pipeline have a PSO and descriptor sets
 *
 * We can't make the PSO immediately because subpasses, but we can (and do) fill out the pipeline create info and
 * allocate descriptor sets
 */
class GraphicsPipeline {
    friend class PipelineCache;
    friend class GraphicsPipelineBuilder;

public:
    VkPipelineLayout get_layout() const;

private:
    std::string pipeline_name;

    std::string vertex_shader_name;

    VkPipelineShaderStageCreateInfo vertex_stage = {};

    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    std::vector<VkVertexInputBindingDescription> vertex_inputs;

    std::vector<VkVertexInputAttributeDescription> vertex_attributes;

    std::string geometry_shader_name;

    tl::optional<VkPipelineShaderStageCreateInfo> geometry_stage = tl::nullopt;

    std::string fragment_shader_name;

    tl::optional<VkPipelineShaderStageCreateInfo> fragment_stage = tl::nullopt;

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {};

    VkPipelineRasterizationStateCreateInfo raster_state = {};

    VkPipelineColorBlendStateCreateFlags blend_flags = {};

    std::vector<VkPipelineColorBlendAttachmentState> blends = {};

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

    // Renderpass and subpass index that this pipeline was most recently used with

    VkRenderPass last_renderpass = VK_NULL_HANDLE;

    uint32_t last_subpass_index;

    VkPipeline pipeline = VK_NULL_HANDLE;

    void create_pipeline_layout(RenderBackend& backend,
                                const std::unordered_map<uint32_t, DescriptorSetInfo>& descriptor_set_infos);
};

using GraphicsPipelineHandle = PooledObject<GraphicsPipeline>;