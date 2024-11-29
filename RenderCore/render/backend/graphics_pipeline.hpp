#pragma once

#include <filesystem>
#include <vector>
#include <cstdint>
#include <unordered_map>

#include <volk.h>
#include <tl/optional.hpp>

#include "render/backend/descriptor_set_info.hpp"
#include "core/object_pool.hpp"

class RenderBackend;

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

    uint32_t get_num_push_constants() const;

    VkShaderStageFlags get_push_constant_shader_stages() const;

    const DescriptorSetInfo& get_descriptor_set_info(uint32_t set_index) const;

    VkPipeline get_pipeline() const;

private:
    std::string pipeline_name;

    VkPipelineCreateFlags flags;

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

    uint32_t num_push_constants = 0;

    VkShaderStageFlags push_constant_stages = 0;

    absl::flat_hash_map<uint32_t, DescriptorSetInfo> descriptor_sets;

    std::vector<VkDescriptorSetLayout> descriptor_set_layouts;

    uint32_t group_index;

    void create_pipeline_layout(
        RenderBackend& backend, const absl::flat_hash_map<uint32_t, DescriptorSetInfo>& descriptor_set_infos,
        const std::vector<VkPushConstantRange>& push_constants
    );
};

using GraphicsPipelineHandle = PooledObject<GraphicsPipeline>;
