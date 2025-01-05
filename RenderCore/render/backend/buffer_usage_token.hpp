#pragma once

#include <volk.h>

#include "render/backend/handles.hpp"

struct BufferUsageToken {
    BufferHandle buffer = nullptr;

    VkPipelineStageFlags2 stage;

    VkAccessFlags2 access;
};

struct BufferBarrier {
    BufferHandle buffer = {};

    BufferUsageToken src = {};

    BufferUsageToken dst = {};

    uint32_t offset = 0;

    uint32_t size = 0;
};
