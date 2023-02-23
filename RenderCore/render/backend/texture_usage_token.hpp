#pragma once

#include <volk.h>

struct TextureUsageToken {
    VkPipelineStageFlags stage;

    VkAccessFlags access;

    VkImageLayout layout;
};
