#pragma once

#include <volk.h>

#include "render/backend/descriptor_set_info.hpp"
#include "core/object_pool.hpp"

struct ComputeShader {
    std::string name;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    uint32_t num_push_constants = 0;
    std::vector<DescriptorSetInfo> descriptor_sets;
};

using ComputePipelineHandle = PooledObject<ComputeShader>;
