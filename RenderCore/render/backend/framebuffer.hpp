#pragma once

#include <optional>
#include <EASTL/vector.h>

#include <volk.h>

#include "handles.hpp"

class RenderBackend;

struct Framebuffer {
    static Framebuffer create(const RenderBackend& backend, const eastl::vector<TextureHandle>& color_attachments,
                              std::optional<TextureHandle> depth_attachment, VkRenderPass render_pass);

    static Framebuffer create(VkDevice device, const eastl::vector<VkImageView>& color_attachments,
                              std::optional<VkImageView> depth_attachment, const VkRect2D& render_area,
                              VkRenderPass render_pass);

    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkRect2D render_area;
};
