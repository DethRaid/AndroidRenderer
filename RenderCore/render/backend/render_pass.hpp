#pragma once

#include <array>
#include <string>
#include <functional>
#include <vector>

#include <tl/optional.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "framebuffer.hpp"
#include "rendering_attachment_info.hpp"
#include "render/backend/buffer_state.hpp"
#include "render/backend/texture_state.hpp"
#include "render/backend/handles.hpp"
#include "render/backend/buffer_usage_token.hpp"
#include "render/backend/texture_usage_token.hpp"
#include "render/backend/descriptor_set_builder.hpp"
#include "render/backend/compute_shader.hpp"

class CommandBuffer;

struct AttachmentBinding {
    TextureHandle texture;
    TextureState state;
    tl::optional<glm::vec4> clear_color;
};

struct ComputePass {
    std::string name;
    
    std::vector<TextureUsageToken> textures;

    std::vector<BufferUsageToken> buffers;

    /**
     * Executes this render pass
     *
     * If this render pass renders to render targets, they're bound before this function is called. The viewport and
     * scissor are set to the dimensions of the render targets, no need to do that manually
     */
    std::function<void(CommandBuffer&)> execute;
};

/**
 * \brief Describes a pass that dispatches a compute shader in a specific way
 */
template<typename PushConstantsType = uint32_t>
struct ComputeDispatch {
    /**
     * \brief Name of this dispatch, for debugging 
     */
    std::string name;

    /**
     * \brief Descriptor sets to bind for this pass. Must contain one entry for every descriptor set that the shader needs
     */
    std::vector<DescriptorSet> descriptor_sets;

    /**
     * \brief Buffers this pass uses that aren't in a descriptor set. Useful for buffers accessed through BDA
     */
    std::vector<BufferUsageToken> buffers;

    /**
     * \brief Push constants for this dispatch. Feel free to reinterpret_cast push_constants.data() into your own type
     */
    PushConstantsType push_constants;

    /**
     * \brief Number of workgroups to dispatch
     */
    glm::uvec3 num_workgroups;

    /**
     * \brief Compute shader to dispatch
     */
    ComputePipelineHandle compute_shader;
};


/**
 * \brief Describes a pass that dispatches a compute shader from an indirect dispatch buffer
 */
template<typename PushConstantsType = uint32_t>
struct IndirectComputeDispatch {
    /**
     * \brief Name of this dispatch, for debugging
     */
    std::string name;

    /**
     * \brief Descriptor sets to bind for this pass. Must contain one entry for every descriptor set that the shader needs
     */
    std::vector<DescriptorSet> descriptor_sets;

    /**
     * \brief Buffers this pass uses that aren't in a descriptor set. Useful for buffers accessed through BDA
     */
    std::vector<BufferUsageToken> buffers;

    /**
     * \brief Push constants for this dispatch. Feel free to reinterpret_cast push_constants.data() into your own type
     */
    PushConstantsType push_constants;

    /**
     * \brief Number of workgroups to dispatch
     */
    BufferHandle dispatch;

    /**
     * \brief Compute shader to dispatch
     */
    ComputePipelineHandle compute_shader;
};

struct TransitionPass {
    std::vector<TextureUsageToken> textures;

    std::vector<BufferUsageToken> buffers;
};

struct BufferCopyPass {
    std::string name;

    BufferHandle dst;

    BufferHandle src;
};


struct ImageCopyPass {
    std::string name;

    TextureHandle dst;

    TextureHandle src;
};

struct Subpass {
    std::string name;

    /**
     * Indices of any input attachments. These indices refer to the render targets in the parent render pass
     */
    std::vector<uint32_t> input_attachments = {};

    /**
     * Indices of any output attachments. These indices refer to the render targets in the parent render pass
     */
    std::vector<uint32_t> color_attachments = {};

    /**
     * Index of the depth attachment. This index refers to the render targets in the parent render pass
     */
    tl::optional<uint32_t> depth_attachment = tl::nullopt;

    std::function<void(CommandBuffer&)> execute;
};

struct RenderPassBeginInfo {
    std::string name;

    std::vector<TextureUsageToken> textures;

    std::vector<BufferUsageToken> buffers;

    /**
     * \brief Descriptor sets that contain sync info we use
     *
     * I need a better name for this
     */
    std::vector<DescriptorSet> descriptor_sets;

    std::vector<TextureHandle> attachments;

    std::vector<VkClearValue> clear_values;

    tl::optional<uint32_t> view_mask;
};

struct RenderPass {
    std::string name;

    std::vector<TextureUsageToken> textures;

    std::vector<BufferUsageToken> buffers;

    std::vector<DescriptorSet> descriptor_sets;
    
    std::vector<TextureHandle> attachments;

    std::vector<VkClearValue> clear_values;

    tl::optional<uint32_t> view_mask;

    std::vector<Subpass> subpasses;
};

struct AttachmentInfo {
    TextureHandle texture;
    VkClearValue clear_value;
};

struct DynamicRenderingPass {
    std::string name;

    std::vector<TextureUsageToken> textures;

    std::vector<BufferUsageToken> buffers;

    std::vector<DescriptorSet> descriptor_sets;

    std::vector<RenderingAttachmentInfo> color_attachments;

    std::optional<RenderingAttachmentInfo> depth_attachment;

    std::optional<TextureHandle> shading_rate_image;

    tl::optional<uint32_t> view_mask;

    std::function<void(CommandBuffer&)> execute;    
};

struct PresentPass {
    TextureHandle swapchain_image;
};
