#include "framebuffer.hpp"

#include <vector>

#include <glm/vec2.hpp>

#include "render/backend/render_backend.hpp"

Framebuffer Framebuffer::create(RenderBackend& backend, const std::vector<TextureHandle>& color_attachments,
                                tl::optional<TextureHandle> depth_attachment, VkRenderPass render_pass) {
    ZoneScoped;

    auto device = backend.get_device();
    auto& allocator = backend.get_global_allocator();

    auto render_area = VkRect2D{};

    auto depth_attachment_count = depth_attachment ? 1 : 0;

    auto attachments = std::vector<VkImageView>{};
    attachments.reserve(color_attachments.size() + depth_attachment_count);

    auto num_layers = 1u;

    for (const auto& color_att: color_attachments) {
        const auto& attachment_actual = allocator.get_texture(color_att);

        attachments.push_back(attachment_actual.rtv);

        // Assumes that all render targets have the same depth
        // If they don't all have the same depth, I'll get very sad
        num_layers = attachment_actual.create_info.extent.depth;

        if (render_area.extent.width == 0) {
            render_area.extent.width = attachment_actual.create_info.extent.width;
            render_area.extent.height = attachment_actual.create_info.extent.height;
        }
    }

    if (depth_attachment) {
        const auto& attachment_actual = allocator.get_texture(*depth_attachment);

        attachments.push_back(attachment_actual.rtv);

        // Assumes that all render targets have the same depth
        // If they don't all have the same depth, I'll get very sad
        num_layers = attachment_actual.create_info.extent.depth;

        if (render_area.extent.width == 0) {
            render_area.extent.width = attachment_actual.create_info.extent.width;
            render_area.extent.height = attachment_actual.create_info.extent.height;
        }
    }

    auto create_info = VkFramebufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),
            .width = render_area.extent.width,
            .height = render_area.extent.height,
            .layers = num_layers,
    };

    auto framebuffer = Framebuffer{.render_area = render_area};
    const auto result = vkCreateFramebuffer(device, &create_info, nullptr, &framebuffer.framebuffer);
    if (result != VK_SUCCESS) {
        throw std::runtime_error{"Could not create framebuffer"};
    }

    return framebuffer;
}

Framebuffer Framebuffer::create(VkDevice device, const std::vector<VkImageView>& color_attachments,
                                tl::optional<VkImageView> depth_attachment, const VkRect2D& render_area,
                                VkRenderPass render_pass) {
    ZoneScoped;

    auto depth_attachment_count = depth_attachment ? 1 : 0;

    auto attachments = std::vector<VkImageView>{};
    attachments.reserve(color_attachments.size() + depth_attachment_count);

    for (const auto& color_att: color_attachments) {
        attachments.push_back(color_att);
    }

    if (depth_attachment) {
        attachments.push_back(*depth_attachment);
    }

    auto create_info = VkFramebufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),
            .width = render_area.extent.width,
            .height = render_area.extent.height,
            .layers = 1,
    };

    auto framebuffer = Framebuffer{.render_area = render_area};
    const auto result = vkCreateFramebuffer(device, &create_info, nullptr, &framebuffer.framebuffer);
    if (result != VK_SUCCESS) {
        throw std::runtime_error{"Could not create framebuffer"};
    }

    return framebuffer;
}
