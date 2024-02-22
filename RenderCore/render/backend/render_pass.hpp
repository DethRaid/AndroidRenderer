#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include <optional>

#include <glm/vec4.hpp>

#include "render/backend/texture_state.hpp"
#include "render/backend/handles.hpp"
#include "render/backend/buffer_usage_token.hpp"
#include "render/backend/texture_usage_token.hpp"

class CommandBuffer;

struct AttachmentBinding {
    TextureHandle texture;
    TextureState state;
    std::optional<glm::vec4> clear_color;
};

struct ComputePass {
    std::string name;
    
    std::unordered_map<TextureHandle, TextureUsageToken> textures;

    std::unordered_map<BufferHandle, BufferUsageToken> buffers;

    /**
     * Executes this render pass
     *
     * If this render pass renders to render targets, they're bound before this function is called. The viewport and
     * scissor are set to the dimensions of the render targets, no need to do that manually
     */
    std::function<void(CommandBuffer&)> execute;
};

struct TransitionPass {
    std::unordered_map<TextureHandle, TextureUsageToken> textures;

    std::unordered_map<BufferHandle, BufferUsageToken> buffers;
};

struct ImageCopyPass {
    std::string name;

    TextureHandle src_image;

    TextureHandle dst_image;
};

struct Subpass {
    std::string name;

    /**
     * Indices of any input attachments. These indices refer to the render targets in the parent render pass
     */
    std::vector<uint32_t> input_attachments;

    /**
     * Indices of any output attachments. These indices refer to the render targets in the parent render pass
     */
    std::vector<uint32_t> color_attachments;

    /**
     * Whether or not to use the depth attachment from the enclosing renderpass
     */
    bool use_depth_attachment;

    std::function<void(CommandBuffer&)> execute;
};

struct RenderPass {
    std::string name;

    std::unordered_map<TextureHandle, TextureUsageToken> textures;

    std::unordered_map<BufferHandle, BufferUsageToken> buffers;
    
    std::vector<TextureHandle> color_attachments;

    std::vector<VkClearValue> color_clear_values;

    std::optional<TextureHandle> depth_attachment = std::nullopt;

    std::optional<VkClearValue> depth_clear_value = std::nullopt;

    std::optional<uint32_t> view_mask;

    std::vector<Subpass> subpasses;
};

struct RenderPassBeginInfo {
    std::string name;

    std::unordered_map<TextureHandle, TextureUsageToken> textures;

    std::unordered_map<BufferHandle, BufferUsageToken> buffers;

    std::vector<TextureHandle> color_attachments;

    std::vector<VkClearValue> color_clear_values;

    std::optional<TextureHandle> depth_attachment = std::nullopt;

    std::optional<VkClearValue> depth_clear_value = std::nullopt;

    std::optional<uint32_t> view_mask;
};

struct PresentPass {
    TextureHandle swapchain_image;
};
