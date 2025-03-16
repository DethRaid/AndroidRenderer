#pragma once

#include <vector>
#include <cstdint>

#include "render/backend/pipeline_interface.hpp"

class RenderBackend;

/**
 * Simple pipeline abstraction
 *
 * Pipeline have a PSO and descriptor sets
 *
 * We can't make the PSO immediately because subpasses, but we can (and do) fill out the pipeline create info and
 * allocate descriptor sets
 */
struct GraphicsPipeline : PipelineBase {
    VkPipeline get_pipeline() const;

    VkPipelineCreateFlags flags;

    std::vector<uint8_t> vertex_shader;

    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    std::vector<VkVertexInputBindingDescription> vertex_inputs;

    std::vector<VkVertexInputAttributeDescription> vertex_attributes;

    std::vector<uint8_t> geometry_shader;

    std::vector<uint8_t> fragment_shader;

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {};

    VkPipelineRasterizationStateCreateInfo raster_state = {};

    VkPipelineColorBlendStateCreateFlags blend_flags = {};

    std::vector<VkPipelineColorBlendAttachmentState> blends = {};

    // Renderpass and subpass index that this pipeline was most recently used with

    VkRenderPass last_renderpass = VK_NULL_HANDLE;

    uint32_t last_subpass_index;

    uint32_t group_index;
};
