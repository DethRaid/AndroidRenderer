#pragma once

#include <cstdint>
#include <vector>
#include <span>

#include <volk.h>
#include <glm/vec2.hpp>
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

#include "render/backend/compute_shader.hpp"
#include "render/backend/rendering_attachment_info.hpp"
#include "render/backend/buffer_usage_token.hpp"
#include "render/backend/handles.hpp"
#include "render/backend/graphics_pipeline.hpp"
#include "render/backend/framebuffer.hpp"

struct DescriptorSet;
struct ComputeShader;
class RenderBackend;

struct RenderingInfo {
    glm::ivec2 render_area_begin;

    glm::uvec2 render_area_size;

    uint32_t layer_count;

    uint32_t view_mask;

    std::vector<RenderingAttachmentInfo> color_attachments;

    std::optional<RenderingAttachmentInfo> depth_attachment;

    std::optional<TextureHandle> shading_rate_image;
};

/**
 * Command buffer abstraction
 *
 * Lets you work with handles and not worry about too much
 */
class CommandBuffer {
public:
    explicit CommandBuffer(VkCommandBuffer vk_cmds, RenderBackend& backend_in);

    void begin() const;


    void set_marker(const std::string& marker_name) const;

    /**
     * Writes some data to a buffer
     *
     * This method makes no attempt to solve for GPU/CPU resource access. You're expected to write to a region of the
     * buffer that's not currently in use
     *
     * @tparam DataType Type of the data to upload
     * @param buffer
     * @param data
     * @param offset Offset in bytes
     */
    template <typename DataType>
    void update_buffer_immediate(BufferHandle buffer, const DataType& data, uint32_t offset = 0);

    void update_buffer_immediate(BufferHandle buffer, const void* data, uint32_t data_size, uint32_t offset = 0) const;

    void flush_buffer(BufferHandle buffer) const;

    // Explicit barrier methods, for when the resource tracking fails

    void barrier(
        BufferHandle buffer, VkPipelineStageFlags source_pipeline_stage, VkAccessFlags source_access,
        VkPipelineStageFlags destination_pipeline_stage, VkAccessFlags destination_access
    ) const;

    /**
     * Issues a batch of pipeline barriers
     */
    void barrier(
        const std::vector<VkMemoryBarrier2>& memory_barriers,
        const std::vector<VkBufferMemoryBarrier2>& buffer_barriers,
        const std::vector<VkImageMemoryBarrier2>& image_barriers
    ) const;

    /**
     * Clears a whole buffer to the specified value
     *
     * @param buffer Buffer to clear
     * @param fill_value  Value to clear the buffer to
     */
    void fill_buffer(BufferHandle buffer, uint32_t fill_value = 0, uint32_t dest_offset = 0) const;

    void fill_buffer(BufferHandle buffer, uint32_t fill_value, uint32_t dest_offset, uint32_t amount_to_write) const;

    void build_acceleration_structures(
        std::span<const VkAccelerationStructureBuildGeometryInfoKHR> build_geometry_infos,
        std::span<VkAccelerationStructureBuildRangeInfoKHR* const> build_range_info_ptrs
    ) const;

    /**
     * Begins a render pass, which implicitly begins the first subpass
     *
     * @param render_pass The render pass to begin
     * @param framebuffer The framebuffer to use with this render pass. Must have the same number of attachments
     * @param clears The clear values for the framebuffer attachments. Must have one entry for every attachment that the
     * render pass clears
     */
    void begin_render_pass(
        VkRenderPass render_pass, const Framebuffer& framebuffer,
        const std::vector<VkClearValue>& clears
    );

    /**
     * Ends the current subpass and begins the next subpass
     */
    void advance_subpass();

    /**
     * Ends the current render pass
     */
    void end_render_pass();

    /**
     * Begins rendering with dynamic rendering
     */
    void begin_rendering(const RenderingInfo& info);

    /**
     * Ends a dynamic render pass
     */
    void end_rendering();

    void set_scissor_rect(const glm::ivec2& upper_left, const glm::ivec2& lower_right) const;

    /**
     * Binds as vertex buffer to a specified vertex input
     *
     * @param binding_index Index of the vertex input to bind to
     * @param buffer Buffer to bind
     */
    void bind_vertex_buffer(uint32_t binding_index, BufferHandle buffer) const;

    template <typename IndexType = uint32_t>
    void bind_index_buffer(BufferHandle buffer) const;

    void set_cull_mode(VkCullModeFlags cull_mode) const;

    void set_front_face(VkFrontFace front_face) const;

    void draw(
        uint32_t num_vertices, uint32_t num_instances = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0
    );

    void draw_indexed(
        uint32_t num_indices, uint32_t num_instances, uint32_t first_index, uint32_t first_vertex,
        uint32_t first_instance
    );

    /**
     * Draws one mesh, pulling draw arguments from the given indirect buffer
     */
    void draw_indirect(BufferHandle indirect_buffer);

    /**
     * Draws one mesh, pulling draw arguments from the given indirect buffer
     */
    void draw_indexed_indirect(BufferHandle indirect_buffer);

    /**
     * \brief Draws many meshes, pulling draw commands from the indirect buffer
     * \param indirect_buffer Buffer of indirect draw commands
     * \param count_buffer Buffer with the count of objects to draw
     * \param max_count Maximum number of objects to draw. Larger values may incur performance penalties on some platforms
     */
    void draw_indexed_indirect(BufferHandle indirect_buffer, BufferHandle count_buffer, uint32_t max_count);

    /**
     * Draws a single triangle
     *
     * Intended for use with a pipeline that renders a fullscreen triangle, such as a postprocessing
     * shader
     */
    void draw_triangle();

    void dispatch_rays(glm::uvec2 dispatch_size);

    /**
     * Executes a buffer of device-generated commands
     */
    void execute_commands();

    void bind_pipeline(const ComputePipelineHandle& pipeline);

    void bind_pipeline(const GraphicsPipelineHandle& pipeline);

    void set_push_constant(uint32_t index, uint32_t data);

    void set_push_constant(uint32_t index, float data);

    /**
     * \brief Binds a buffer to the push constants starting at index using buffer device address
     *
     * Note that this uses push constant space to bind the buffer. Calls to `set_push_constant` should be careful to
     * not overwrite buffer addresses
     *
     * \param index Index at which to bind the buffer. Must be evenly divisible by two
     * \param buffer_handle Handle of the buffer to bind
     */
    void bind_buffer_reference(uint32_t index, BufferHandle buffer_handle);

    void bind_descriptor_set(uint32_t set_index, const DescriptorSet& set);

    void bind_descriptor_set(uint32_t set_index, VkDescriptorSet set);

    void clear_descriptor_set(uint32_t set_index);

    void dispatch(uint32_t width, uint32_t height, uint32_t depth);

    void dispatch_indirect(BufferHandle indirect_buffer);

    void copy_buffer_to_buffer(BufferHandle dst, uint32_t dst_offset, BufferHandle src, uint32_t src_offset) const;

    void copy_image_to_image(TextureHandle src, TextureHandle dst) const;

    void reset_event(VkEvent event, VkPipelineStageFlags stages) const;

    void set_event(VkEvent event, const std::vector<BufferBarrier>& buffers);

    void wait_event(VkEvent);

    void begin_label(const std::string& event_name) const;

    void end_label() const;

    void end() const;

#if defined(TRACY_ENABLE)
    tracy::VkCtx* get_tracy_context() const;
#endif

    VkCommandBuffer get_vk_commands() const;

    VkRenderPass get_current_renderpass() const;

    uint32_t get_current_subpass() const;

    RenderBackend& get_backend() const;

private:
    VkCommandBuffer commands;

    RenderBackend* backend;

    VkRenderPass current_render_pass = VK_NULL_HANDLE;

    Framebuffer current_framebuffer = {};

    uint32_t bound_view_mask = 0;

    std::vector<VkFormat> bound_color_attachment_formats;

    std::optional<VkFormat> bound_depth_attachment_format;

    bool using_fragment_shading_rate_attachment = false;

    uint32_t current_subpass = 0;

    std::array<uint32_t, 128> push_constants = {};

    std::array<VkDescriptorSet, 8> descriptor_sets = {};

    VkPipelineBindPoint current_bind_point = {};

    VkPipelineLayout current_pipeline_layout = VK_NULL_HANDLE;

    VkShaderStageFlags push_constant_shader_stages = 0;

    uint32_t num_push_constants_in_current_pipeline = 0;

    bool are_bindings_dirty = false;

    /**
     * Cache of buffer barriers for events
     *
     * The spec states that the dependency info for each set/wait event call for the same event must match. The backend
     * should handle that noise
     */
    std::unordered_map<VkEvent, std::vector<VkBufferMemoryBarrier2>> event_buffer_barriers;
    uint32_t num_descriptor_sets_in_current_pipeline = 0;

    void bind_index_buffer(BufferHandle buffer, VkIndexType index_type) const;

    void commit_bindings();
};

template <typename DataType>
void CommandBuffer::update_buffer_immediate(BufferHandle buffer, const DataType& data, const uint32_t offset) {
    update_buffer_immediate(buffer, &data, sizeof(DataType), offset);
}

template <typename IndexType>
void CommandBuffer::bind_index_buffer(const BufferHandle buffer) const {
    auto index_type = VK_INDEX_TYPE_NONE_KHR;
    if constexpr(sizeof(IndexType) == sizeof(uint32_t)) {
        index_type = VK_INDEX_TYPE_UINT32;
    } else if constexpr(sizeof(IndexType) == sizeof(uint16_t)) {
        index_type = VK_INDEX_TYPE_UINT16;
    } else {
        throw std::runtime_error{"Invalid index type"};
    }

    bind_index_buffer(buffer, index_type);
}


#define GpuZoneScopedN(commands, name) ZoneScopedN(name); TracyVkZone((commands).get_tracy_context(), (commands).get_vk_commands(), name)

#define GpuZoneScoped(commands) GpuZoneScopedN(commands, __func__)
