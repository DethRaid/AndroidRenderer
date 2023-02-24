#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include <tl/optional.hpp>
#include <glm/vec4.hpp>

#include "framebuffer.hpp"
#include "render/backend/buffer_state.hpp"
#include "render/backend/texture_state.hpp"
#include "render/backend/handles.hpp"
#include "render/backend/buffer_usage_token.hpp"
#include "render/backend/texture_usage_token.hpp"

class CommandBuffer;

struct AttachmentBinding {
    TextureHandle texture;
    TextureState state;
    tl::optional<glm::vec4> clear_color;
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

struct Subpass {
    std::string name;

    std::function<void(CommandBuffer&)> execute;
};

struct RenderPass {
    std::string name;

    std::unordered_map<TextureHandle, TextureUsageToken> textures;

    std::unordered_map<BufferHandle, BufferUsageToken> buffers;

    VkRenderPass render_pass;

    std::vector<TextureHandle> render_targets;

    tl::optional<TextureHandle> depth_target;

    std::vector<VkClearValue> clear_values;

    std::vector<Subpass> subpasses;
};
