#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include <tl/optional.hpp>
#include <glm/vec4.hpp>

#include "render/backend/buffer_state.hpp"
#include "render/backend/texture_state.hpp"
#include "render/backend/handles.hpp"

class CommandBuffer;

struct AttachmentBinding {
    TextureHandle texture;
    TextureState state;
    tl::optional<glm::vec4> clear_color;
};

struct RenderPass {
    std::string name;

    /**
     * All the render targets to render to in this render pass
     *
     * If this list is empty, the pass is either a transfer or a compute pass
     *
     * The render targets with a color state are bound in the order they appear, then the depth target is bound to the
     * depth slot (if present)
     *
     * If a depth target, the red of the clear color is the clear depth
     */
    std::vector<AttachmentBinding> attachments;

    /**
     * Non-render target textures this pass weill use
     *
     * Useful for compute passes that render to a texture, or render passes that read from a render target
     *
     * May or may not be useful for input attachments
     *
     * You don't need to track material textures with this if you know that they're already in the proper layout
     */
    std::unordered_map<TextureHandle, TextureState> textures;

    std::unordered_map<BufferHandle, BufferState> buffers;

    /**
     * Executes this render pass
     *
     * If this render pass renders to render targets, they're bound before this function is called. The viewport and
     * scissor are set to the dimensions of the render targets, no need to do that manually
     */
    std::function<void(CommandBuffer&)> execute;
};



