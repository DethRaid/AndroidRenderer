#pragma once

#include <absl/container/flat_hash_map.h>
#include <volk.h>

struct TextureUsageToken {
    VkPipelineStageFlags2 stage;

    VkAccessFlags2 access;

    VkImageLayout layout;
};

using TextureUsageMap = absl::flat_hash_map<TextureHandle, TextureUsageToken>;
