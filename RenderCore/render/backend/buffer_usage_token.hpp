#pragma once

#include <unordered_map>

#include <volk.h>

struct BufferUsageToken {
    VkPipelineStageFlags2KHR stage;

    VkAccessFlags2KHR access;
};

using BufferUsageMap = std::unordered_map<BufferHandle, BufferUsageToken>;
