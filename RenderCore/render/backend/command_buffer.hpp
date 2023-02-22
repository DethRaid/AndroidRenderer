#pragma once

#include <cstdint>
#include <vector>
#include <span>

#include <volk.h>
#include <tracy/TracyVulkan.hpp>

#include "render/backend/handles.hpp"
#include "render/backend/pipeline.hpp"
#include "render/backend/framebuffer.hpp"
#include "render/backend/vk_descriptors.hpp"
#include "compute_shader.hpp"

class RenderBackend;

using BufferUsageMap = std::unordered_map<BufferHandle, std::pair<VkPipelineStageFlags, VkAccessFlags>>;

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
     * @tparam DataType
     * @param buffer
     * @param data
     * @param offset Offset in bytes
     */
    template<typename DataType>
    void update_buffer(BufferHandle buffer, const DataType& data, uint32_t offset = 0);

    void update_buffer(BufferHandle buffer, const void* data, uint32_t data_size, uint32_t offset = 0);

    /**
     * Ensures that the resource can be accessed in the specified way
     *
     * This method ensures that the given resource can be accessed by future commands in the specified way. It may or
     * may not issue a barrier if needed, or it may only update the internal state tracking information.
     *
     * @param resource
     * @param pipeline_stage
     * @param access
     */
    void set_resource_usage(BufferHandle resource, VkPipelineStageFlags pipeline_stage, VkAccessFlags access);

    void flush_buffer(BufferHandle buffer);

    // Explicit barrier methods, for when the resource tracking fails

    void barrier(BufferHandle buffer, VkPipelineStageFlags source_pipeline_stage, VkAccessFlags source_access,
                 VkPipelineStageFlags destination_pipeline_stage, VkAccessFlags destination_access);

    /**
     * Executes a barrier for all mip levels of an image
     *
     * @param texture
     * @param source_pipeline_stage
     * @param source_access
     * @param old_layout
     * @param destination_pipeline_stage
     * @param destination_access
     * @param new_layout
     */
    void barrier(TextureHandle texture, const VkPipelineStageFlags source_pipeline_stage,
                 const VkAccessFlags source_access, const VkImageLayout old_layout,
                 const VkPipelineStageFlags destination_pipeline_stage, const VkAccessFlags destination_access,
                 const VkImageLayout new_layout);

    /**
     * Clears a whole buffer to the specified value
     *
     * @param buffer Buffer to clear
     * @param fill_value  Value to clear the buffer to
     */
    void fill_buffer(BufferHandle buffer, uint32_t fill_value = 0);

    /**
     * Begins a render pass, which implicitly begins the first subpass
     *
     * @param render_pass The render pass to begin
     * @param framebuffer The framebuffer to use with this render pass. Must have the same number of attachments
     * @param clears The clear values for the framebuffer attachments. Must have one entry for every attachment that the
     * render pass clears
     */
    void begin_render_pass(VkRenderPass render_pass, const Framebuffer& framebuffer,
                           const std::vector<VkClearValue>& clears);

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
    void bind_vertex_buffer(uint32_t binding_index, BufferHandle buffer);

    void bind_index_buffer(BufferHandle buffer);

    void draw_indexed(uint32_t num_indices, uint32_t num_instances, uint32_t first_index, uint32_t first_vertex,
                      uint32_t first_instance);

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

    void bind_descriptor_set(uint32_t set_index, VkDescriptorSet set);

    void clear_descriptor_set(uint32_t set_index);

    void dispatch(uint32_t width, uint32_t height, uint32_t depth);

    void end();

    tracy::VkCtx* const get_tracy_context() const;

    VkCommandBuffer get_vk_commands() const;

    const BufferUsageMap& get_initial_buffer_usages() const;

    const BufferUsageMap& get_final_buffer_usages() const;

    VkRenderPass get_current_renderpass() const;

    uint32_t get_current_subpass() const;

    const RenderBackend& get_backend() const;

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

    BufferUsageMap initial_buffer_usages;

    BufferUsageMap last_buffer_usages;

    void commit_bindings();
};

template<typename DataType>
void CommandBuffer::update_buffer(BufferHandle buffer, const DataType &data, const uint32_t offset) {
    update_buffer(buffer, &data, sizeof(DataType), offset);
}


#define GpuZoneScopedN(commands, name) TracyVkZone(commands.get_tracy_context(), commands.get_vk_commands(), name)

#define GpuZoneScoped(commands) GpuZoneScopedN(commands, __func__)
