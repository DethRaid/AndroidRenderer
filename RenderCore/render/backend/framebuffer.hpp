#pragma once

#include <optional>
#include <vector>

#include <volk.h>

#include "handles.hpp"

class RenderBackend;

struct Framebuffer {
    static Framebuffer create(RenderBackend& backend, const std::vector<TextureHandle>& color_attachments,
                              std::optional<TextureHandle> depth_attachment, VkRenderPass render_pass);

    static Framebuffer create(VkDevice device, const std::vector<VkImageView>& color_attachments,
                              std::optional<VkImageView> depth_attachment, const VkRect2D& render_area,
                              VkRenderPass render_pass);

    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkRect2D render_area;
};
