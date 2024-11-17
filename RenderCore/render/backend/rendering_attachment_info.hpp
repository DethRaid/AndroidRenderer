#pragma once

#include <vulkan/vulkan_core.h>

#include "render/backend/handles.hpp"

struct RenderingAttachmentInfo {
    TextureHandle image;

    VkAttachmentLoadOp load_op;
    VkAttachmentStoreOp store_op;
    VkClearValue clear_value;
};
