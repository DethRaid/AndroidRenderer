#pragma once

#include <cstdint>
#include <vector>
#include <span>

#include <volk.h>
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

#include "buffer_usage_token.hpp"
#include "render/backend/handles.hpp"
#include "render/backend/pipeline.hpp"
#include "render/backend/framebuffer.hpp"
#include "render/backend/vk_descriptors.hpp"

struct ComputeShader;
class RenderBackend;

/**
 * Command buffer abstraction
 *
 * Lets you work with handles and not worry about too much
 */
class CommandBuffer {
public:
    explicit CommandBuffer(VkCommandBuffer vk_cmds, RenderBackend& backend_in);

    void begin();


    void set_marker(const std::string& marker_name);

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
    void update_buffer(BufferHandle buffer, const DataType& data, uint32_t offset = 0);

    void update_buffer(BufferHandle buffer, const void* data, uint32_t data_size, uint32_t offset = 0);

    void flush_buffer(BufferHandle buffer);

    // Explicit barrier methods, for when the resource tracking fails

    void barrier(
        BufferHandle buffer, VkPipelineStageFlags source_pipeline_stage, VkAccessFlags source_access,
        VkPipelineStageFlags destination_pipeline_stage, VkAccessFlags destination_access
    );

    /**
     * Executes a barrier for all mip levels of an image
     */
    void barrier(
        TextureHandle texture, const VkPipelineStageFlags source_pipeline_stage,
        const VkAccessFlags source_access, const VkImageLayout old_layout,
        const VkPipelineStageFlags destination_pipeline_stage, const VkAccessFlags destination_access,
        const VkImageLayout new_layout
    );

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
    void fill_buffer(BufferHandle buffer, uint32_t fill_value = 0) const;

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
     * Binds as vertex buffer to a specified vertex input
     *
     * @param binding_index Index of the vertex input to bind to
     * @param buffer Buffer to bind
     */
    void bind_vertex_buffer(uint32_t binding_index, BufferHandle buffer) const;

    void bind_index_buffer(BufferHandle buffer) const;

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
     * Draws a single triangle
     *
     * Intended for use with a pipeline that renders a fullscreen triangle, such as a postprocessing
     * shader
     */
    void draw_triangle();

    void bind_shader(const ComputeShader& shader);

    void bind_pipeline(Pipeline& pipeline);

    void set_push_constant(uint32_t index, uint32_t data);

    void set_push_constant(uint32_t index, float data);

    void bind_descriptor_set(uint32_t set_index, VkDescriptorSet set);

    void clear_descriptor_set(uint32_t set_index);

    void dispatch(uint32_t width, uint32_t height, uint32_t depth);

    void reset_event(VkEvent event, VkPipelineStageFlags stages) const;

    void set_event(VkEvent event, const std::vector<BufferBarrier>& buffers);
    
    void wait_event(VkEvent);

    void begin_label(const std::string& event_name) const;

    void end_label() const;

    void end() const;

#if TRACY_ENABLE
    tracy::VkCtx* const get_tracy_context() const;
#endif

    VkCommandBuffer get_vk_commands() const;

    VkRenderPass get_current_renderpass() const;

    uint32_t get_current_subpass() const;

    RenderBackend& get_backend() const;

private:
    VkCommandBuffer commands;

    RenderBackend* backend;

    VkRenderPass current_render_pass = VK_NULL_HANDLE;

    Framebuffer current_framebuffer;

    uint32_t current_subpass = 0;

    std::array<uint32_t, 8> push_constants = {0, 0, 0, 0, 0, 0, 0, 0};

    std::array<VkDescriptorSet, 8> descriptor_sets = {};

    VkPipelineBindPoint current_bind_point = {};

    VkPipelineLayout current_pipeline_layout = VK_NULL_HANDLE;

    bool are_bindings_dirty = false;

    /**
     * Cache of buffer barriers for events
     *
     * The spec states that the dependency info for each set/wait event call for the same event must match. The backend
     * should handle that noise
     */
    std::unordered_map<VkEvent, std::vector<VkBufferMemoryBarrier2>> event_buffer_barriers;

    void commit_bindings();
};

template <typename DataType>
void CommandBuffer::update_buffer(BufferHandle buffer, const DataType& data, const uint32_t offset) {
    update_buffer(buffer, &data, sizeof(DataType), offset);
}


#define GpuZoneScopedN(commands, name) ZoneScopedN(name); TracyVkZone((commands).get_tracy_context(), (commands).get_vk_commands(), name)

#define GpuZoneScoped(commands) GpuZoneScopedN(commands, __func__)
