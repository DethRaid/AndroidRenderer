#pragma once

#include <vulkan/vulkan_core.h>

#include "render/backend/handles.hpp"

struct RenderingAttachmentInfo {
    TextureHandle image;

    VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_LOAD;
    VkAttachmentStoreOp store_op = VK_ATTACHMENT_STORE_OP_STORE;
    VkClearValue clear_value = {};
};
