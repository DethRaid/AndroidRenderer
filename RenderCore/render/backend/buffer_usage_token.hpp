#pragma once

#include <unordered_map>

#include <volk.h>

#include "render/backend/handles.hpp"

struct BufferUsageToken {
    VkPipelineStageFlags2 stage;

    VkAccessFlags2 access;
};

struct BufferBarrier {
    BufferHandle buffer = BufferHandle::None;

    BufferUsageToken src = {};

    BufferUsageToken dst = {};

    uint32_t offset = 0;

    uint32_t size = 0;
};

using BufferUsageMap = std::unordered_map<BufferHandle, BufferUsageToken>;
