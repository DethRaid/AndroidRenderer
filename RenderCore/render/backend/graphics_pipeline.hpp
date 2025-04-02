#pragma once

#include <EASTL/fixed_vector.h>
#include <EASTL/vector.h>
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

    eastl::vector<std::byte> vertex_shader;

    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    eastl::fixed_vector<VkVertexInputBindingDescription, 8> vertex_inputs;

    eastl::fixed_vector<VkVertexInputAttributeDescription, 8> vertex_attributes;

    eastl::vector<std::byte> geometry_shader;

    eastl::vector<std::byte> fragment_shader;

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {};

    VkPipelineRasterizationStateCreateInfo raster_state = {};

    VkPipelineColorBlendStateCreateFlags blend_flags = {};

    eastl::fixed_vector<VkPipelineColorBlendAttachmentState, 8> blends = {};

    uint32_t group_index;
};
