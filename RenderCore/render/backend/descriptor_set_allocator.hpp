#pragma once

#include "render/backend/compute_shader.hpp"
#include "render/backend/graphics_pipeline.hpp"
#include "render/backend/descriptor_set_builder.hpp"
#include "render/backend/vk_descriptors.hpp"

class DescriptorSetAllocator : public vkutil::DescriptorAllocator {
public:
    explicit DescriptorSetAllocator(RenderBackend& backend_in);

    DescriptorSet create_set(GraphicsPipelineHandle pipeline, uint32_t set_index);

    DescriptorSet create_set(ComputePipelineHandle pipeline, uint32_t set_index);

private:
    RenderBackend* backend;
};

