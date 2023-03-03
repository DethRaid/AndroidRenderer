#pragma once

#include <volk.h>

struct TextureUsageToken {
    VkPipelineStageFlags2KHR stage;

    VkAccessFlags2KHR access;

    VkImageLayout layout;
};
