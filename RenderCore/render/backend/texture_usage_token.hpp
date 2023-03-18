#pragma once

#include <unordered_map>

#include <volk.h>

struct TextureUsageToken {
    VkPipelineStageFlags2KHR stage;

    VkAccessFlags2KHR access;

    VkImageLayout layout;
};

using TextureUsageMap = std::unordered_map<TextureHandle, TextureUsageToken>;
