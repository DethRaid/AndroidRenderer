#pragma once

#include <volk.h>

#include <EASTL/fixed_vector.h>

#include "render/backend/handles.hpp"

struct BufferUsageToken {
    BufferHandle buffer = nullptr;

    VkPipelineStageFlags2 stage;

    VkAccessFlags2 access;
};

using BufferUsageList = eastl::fixed_vector<BufferUsageToken, 32>;

struct BufferBarrier {
    BufferHandle buffer = {};

    BufferUsageToken src = {};

    BufferUsageToken dst = {};

    uint32_t offset = 0;

    uint32_t size = 0;
};
