#pragma once

#include <volk.h>

#include "render/backend/descriptor_set_info.hpp"
#include "core/object_pool.hpp"

struct ComputeShader {
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    uint32_t num_push_constants = 0;
    std::unordered_map<uint32_t, DescriptorSetInfo> descriptor_sets;
};

using ComputePipelineHandle = PooledObject<ComputeShader>;
