#pragma once

#include <volk.h>

#include "render/backend/handles.hpp"

struct TextureUsageToken {
    TextureHandle texture = nullptr;

    VkPipelineStageFlags2 stage;

    VkAccessFlags2 access;

    VkImageLayout layout;
};
