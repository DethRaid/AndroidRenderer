#pragma once

#include <volk.h>

#include <EASTL/fixed_vector.h>

#include "render/backend/handles.hpp"

struct TextureUsageToken {
    TextureHandle texture = nullptr;

    VkPipelineStageFlags2 stage;

    VkAccessFlags2 access;

    VkImageLayout layout;
};

using TextureUsageList = eastl::fixed_vector<TextureUsageToken, 32>;
