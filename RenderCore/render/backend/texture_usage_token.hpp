#pragma once

#include <unordered_map>

#include <volk.h>

struct TextureUsageToken {
    VkPipelineStageFlags2 stage;

    VkAccessFlags2 access;

    VkImageLayout layout;
};

using TextureUsageMap = std::unordered_map<TextureHandle, TextureUsageToken>;
