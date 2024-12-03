#include "framebuffer.hpp"

#include <vector>

#include <glm/vec2.hpp>
#include <tracy/Tracy.hpp>

#include "render/backend/render_backend.hpp"

Framebuffer Framebuffer::create(const RenderBackend& backend, const std::vector<TextureHandle>& color_attachments,
                                const std::optional<TextureHandle> depth_attachment, const VkRenderPass render_pass) {
    ZoneScoped;

    auto device = backend.get_device();
    auto& allocator = backend.get_global_allocator();

    auto render_area = VkRect2D{};

    auto attachments = std::array<VkImageView, 9>{};
    auto attachment_write_idx = 0u;

    auto num_layers = 1u;

    {
        ZoneScopedN("Collect attachments from TextureHandles");
        for (const auto& color_att : color_attachments) {
            attachments[attachment_write_idx] = color_att->attachment_view;
            attachment_write_idx++;

            // Assumes that all render targets have the same depth
            // If they don't all have the same depth, I'll get very sad
            num_layers = color_att->create_info.extent.depth;

            if (render_area.extent.width == 0) {
                render_area.extent.width = color_att->create_info.extent.width;
                render_area.extent.height = color_att->create_info.extent.height;
            }
        }

        if (depth_attachment) {
            auto depth_handle = *depth_attachment;
            attachments[attachment_write_idx] = depth_handle->attachment_view;
            attachment_write_idx++;

            // Assumes that all render targets have the same depth
            // If they don't all have the same depth, I'll get very sad
            num_layers = depth_handle->create_info.extent.depth;

            if (render_area.extent.width == 0) {
                render_area.extent.width = depth_handle->create_info.extent.width;
                render_area.extent.height = depth_handle->create_info.extent.height;
            }
        }
    }

    auto create_info = VkFramebufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = attachment_write_idx,
            .pAttachments = attachments.data(),
            .width = render_area.extent.width,
            .height = render_area.extent.height,
            .layers = num_layers,
    };

    auto framebuffer = Framebuffer{.render_area = render_area};
    {
        ZoneScopedN("vkCreateFramebuffer");
        const auto result = vkCreateFramebuffer(device, &create_info, nullptr, &framebuffer.framebuffer);
        if (result != VK_SUCCESS) {
            throw std::runtime_error{ "Could not create framebuffer" };
        }
    }

    return framebuffer;
}

Framebuffer Framebuffer::create(const VkDevice device, const std::vector<VkImageView>& color_attachments,
                                const std::optional<VkImageView> depth_attachment, const VkRect2D& render_area,
                                const VkRenderPass render_pass) {
    ZoneScoped;

    auto depth_attachment_count = depth_attachment ? 1 : 0;

    auto attachments = std::vector<VkImageView>{};
    attachments.reserve(color_attachments.size() + depth_attachment_count);

    {
        ZoneScopedN("Collect attachments");
        for (const auto& color_att : color_attachments) {
            attachments.push_back(color_att);
        }

        if (depth_attachment) {
            attachments.push_back(*depth_attachment);
        }
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
    {
        ZoneScopedN("vkCreateFramebuffer");
        const auto result = vkCreateFramebuffer(device, &create_info, nullptr, &framebuffer.framebuffer);
        if (result != VK_SUCCESS) {
            throw std::runtime_error{ "Could not create framebuffer" };
        }
    }

    return framebuffer;
}
