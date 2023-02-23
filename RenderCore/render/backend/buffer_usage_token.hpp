#pragma once

#include <volk.h>

struct BufferUsageToken {
    VkPipelineStageFlags stage;

    VkAccessFlags access;
};
