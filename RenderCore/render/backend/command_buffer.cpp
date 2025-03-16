#include "command_buffer.hpp"

#include <glm/common.hpp>

#include "pipeline_cache.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/gpu_texture.hpp"
#include "utils.hpp"
#include "console/cvars.hpp"
#include "core/system_interface.hpp"

static std::shared_ptr<spdlog::logger> logger;

[[maybe_unused]] static AutoCVar_Int cvar_validate_bindings{
    "r.Debug.ValidateBindings",
    "Whether or not to validate bindings, such as vertex or index buffers",
    1
};

CommandBuffer::CommandBuffer(const VkCommandBuffer vk_cmds, RenderBackend& backend_in) :
    commands{vk_cmds}, backend{&backend_in} {
    if(logger == nullptr) {
        logger = SystemInterface::get().get_logger("CommandBuffer");
        logger->set_level(spdlog::level::debug);
    }
    for(auto& set : descriptor_sets) {
        set = VK_NULL_HANDLE;
    }
}

void CommandBuffer::begin() const {
    constexpr auto begin_info = VkCommandBufferBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(commands, &begin_info);
}

void CommandBuffer::set_marker(const std::string& marker_name) const {
    if(vkCmdSetCheckpointNV != nullptr) {
        vkCmdSetCheckpointNV(commands, marker_name.c_str());
    }
}

void
CommandBuffer::update_buffer_immediate(
    const BufferHandle buffer, const void* data, const uint32_t data_size, const uint32_t offset
) const {
    auto* write_ptr = static_cast<uint8_t*>(buffer->allocation_info.pMappedData) + offset;

    std::memcpy(write_ptr, data, data_size);

    flush_buffer(buffer);
}

void CommandBuffer::flush_buffer(const BufferHandle buffer) const {
    auto& resources = backend->get_global_allocator();

    vmaFlushAllocation(resources.get_vma(), buffer->allocation, 0, VK_WHOLE_SIZE);
}

void CommandBuffer::barrier(
    const BufferHandle buffer, const VkPipelineStageFlags source_pipeline_stage,
    const VkAccessFlags source_access,
    const VkPipelineStageFlags destination_pipeline_stage,
    const VkAccessFlags destination_access
) const {
    const auto barrier = VkBufferMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = source_access,
        .dstAccessMask = destination_access,
        .buffer = buffer->buffer,
        .offset = 0,
        .size = buffer->create_info.size
    };

    // V0: Issue the barrier immediately
    vkCmdPipelineBarrier(
        commands,
        source_pipeline_stage,
        destination_pipeline_stage,
        0,
        0,
        nullptr,
        1,
        &barrier,
        0,
        nullptr
    );

    // V1: Batch the barriers
}

void CommandBuffer::barrier(
    const std::vector<VkMemoryBarrier2>& memory_barriers, const std::vector<VkBufferMemoryBarrier2>& buffer_barriers,
    const std::vector<VkImageMemoryBarrier2>& image_barriers
) const {
    const auto dependency_info = VkDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = static_cast<uint32_t>(memory_barriers.size()),
        .pMemoryBarriers = memory_barriers.data(),
        .bufferMemoryBarrierCount = static_cast<uint32_t>(buffer_barriers.size()),
        .pBufferMemoryBarriers = buffer_barriers.data(),
        .imageMemoryBarrierCount = static_cast<uint32_t>(image_barriers.size()),
        .pImageMemoryBarriers = image_barriers.data()
    };
    vkCmdPipelineBarrier2(commands, &dependency_info);
}

void CommandBuffer::fill_buffer(
    const BufferHandle buffer, const uint32_t fill_value, const uint32_t dest_offset
) const {
    fill_buffer(buffer, fill_value, dest_offset, buffer->create_info.size - dest_offset);
}

void CommandBuffer::fill_buffer(
    const BufferHandle buffer, const uint32_t fill_value, const uint32_t dest_offset, const uint32_t amount_to_write
) const {
    vkCmdFillBuffer(commands, buffer->buffer, dest_offset, amount_to_write, fill_value);
}

void CommandBuffer::build_acceleration_structures(
    const std::span<const VkAccelerationStructureBuildGeometryInfoKHR> build_geometry_infos,
    const std::span<VkAccelerationStructureBuildRangeInfoKHR* const> build_range_info_ptrs
) const {
    vkCmdBuildAccelerationStructuresKHR(
        commands,
        static_cast<uint32_t>(build_geometry_infos.size()),
        build_geometry_infos.data(),
        build_range_info_ptrs.data());
}

void CommandBuffer::begin_rendering(const RenderingInfo& info) {
    auto attachment_infos = std::vector<VkRenderingAttachmentInfo>{};
    attachment_infos.reserve(
        info.color_attachments.size() +
        (info.depth_attachment.has_value() ? 1 : 0)
    );

    bound_color_attachment_formats.reserve(info.color_attachments.size());

    for(const auto& color_attachment : info.color_attachments) {
        attachment_infos.emplace_back(
            VkRenderingAttachmentInfo{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = color_attachment.image->attachment_view,
                .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .loadOp = color_attachment.load_op,
                .storeOp = color_attachment.store_op,
                .clearValue = color_attachment.clear_value,
            }
        );

        bound_color_attachment_formats.emplace_back(color_attachment.image->create_info.format);
    }

    VkRenderingAttachmentInfo* depth_attachment_ptr = nullptr;
    if(info.depth_attachment) {
        attachment_infos.emplace_back(
            VkRenderingAttachmentInfo{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = info.depth_attachment->image->attachment_view,
                .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .loadOp = info.depth_attachment->load_op,
                .storeOp = info.depth_attachment->store_op,
                .clearValue = info.depth_attachment->clear_value,
            }
        );
        depth_attachment_ptr = &attachment_infos.back();
        bound_depth_attachment_format = info.depth_attachment->image->create_info.format;
    }

    auto rendering_info = VkRenderingInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .offset = {.x = info.render_area_begin.x, .y = info.render_area_begin.y},
            .extent = {.width = info.render_area_size.x, .height = info.render_area_size.y}
        },
        .layerCount = info.layer_count,
        .viewMask = info.view_mask,
        .colorAttachmentCount = static_cast<uint32_t>(info.color_attachments.size()),
        .pColorAttachments = attachment_infos.data(),
        .pDepthAttachment = depth_attachment_ptr,
    };

    auto shading_rate_info = VkRenderingFragmentShadingRateAttachmentInfoKHR{
        .sType = VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR,
    };
    if(info.shading_rate_image) {
        shading_rate_info.imageView = info.shading_rate_image.value()->image_view;
        shading_rate_info.imageLayout = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;

        const auto texel_size = glm::uvec2{RenderBackend::get().get_max_shading_rate_texel_size()};
        shading_rate_info.shadingRateAttachmentTexelSize.width = texel_size.x;
        shading_rate_info.shadingRateAttachmentTexelSize.height = texel_size.y;

        rendering_info.pNext = &shading_rate_info;

        using_fragment_shading_rate_attachment = true;
    }

    bound_view_mask = info.view_mask;

    vkCmdBeginRendering(commands, &rendering_info);

    const auto viewport = VkViewport{
        .x = static_cast<float>(info.render_area_begin.x),
        .y = static_cast<float>(info.render_area_begin.y),
        .width = static_cast<float>(info.render_area_size.x),
        .height = static_cast<float>(info.render_area_size.y),
        .minDepth = 0,
        .maxDepth = 1
    };
    vkCmdSetViewport(commands, 0, 1, &viewport);

    vkCmdSetScissor(commands, 0, 1, &rendering_info.renderArea);
}

void CommandBuffer::end_rendering() {
    vkCmdEndRendering(commands);

    bound_color_attachment_formats.clear();
    bound_depth_attachment_format = std::nullopt;
    bound_view_mask = 0;
    using_fragment_shading_rate_attachment = false;
}

void CommandBuffer::set_scissor_rect(const glm::ivec2& upper_left, const glm::ivec2& lower_right) const {
    const auto scissor_rect = VkRect2D{
        .offset = {.x = upper_left.x, .y = upper_left.y},
        .extent = {
            .width = static_cast<uint32_t>(lower_right.x - upper_left.x),
            .height = static_cast<uint32_t>(lower_right.y - upper_left.y)
        }
    };
    vkCmdSetScissor(commands, 0, 1, &scissor_rect);
}

void CommandBuffer::bind_vertex_buffer(const uint32_t binding_index, const BufferHandle buffer) const {
    constexpr auto offset = VkDeviceSize{0};

    vkCmdBindVertexBuffers(commands, binding_index, 1, &buffer->buffer, &offset);
}

void CommandBuffer::set_cull_mode(const VkCullModeFlags cull_mode) const {
    vkCmdSetCullMode(commands, cull_mode);
}

void CommandBuffer::set_front_face(const VkFrontFace front_face) const {
    vkCmdSetFrontFace(commands, front_face);
}

void CommandBuffer::draw(
    const uint32_t num_vertices, const uint32_t num_instances, const uint32_t first_vertex,
    const uint32_t first_instance
) {
    commit_bindings();

    vkCmdDraw(commands, num_vertices, num_instances, first_vertex, first_instance);
}

void CommandBuffer::draw_indexed(
    const uint32_t num_indices, const uint32_t num_instances,
    const uint32_t first_index,
    const uint32_t first_vertex, const uint32_t first_instance
) {
    commit_bindings();

    vkCmdDrawIndexed(
        commands,
        num_indices,
        num_instances,
        first_index,
        static_cast<int32_t>(first_vertex),
        first_instance
    );
}

void CommandBuffer::draw_indirect(const BufferHandle indirect_buffer) {
    commit_bindings();

    vkCmdDrawIndirect(commands, indirect_buffer->buffer, 0, 1, 0);
}

void CommandBuffer::draw_indexed_indirect(const BufferHandle indirect_buffer) {
    commit_bindings();

    vkCmdDrawIndexedIndirect(commands, indirect_buffer->buffer, 0, 1, 0);
}

void CommandBuffer::draw_indexed_indirect(
    const BufferHandle indirect_buffer, const BufferHandle count_buffer, const uint32_t max_count
) {
    commit_bindings();

    vkCmdDrawIndexedIndirectCount(
        commands,
        indirect_buffer->buffer,
        0,
        count_buffer->buffer,
        0,
        max_count,
        sizeof(VkDrawIndexedIndirectCommand)
    );
}

void CommandBuffer::draw_triangle() {
    set_cull_mode(VK_CULL_MODE_NONE);

    commit_bindings();

    vkCmdDraw(commands, 3, 1, 0, 0);
}

void CommandBuffer::dispatch_rays(const glm::uvec2 dispatch_size) {
    commit_bindings();

   //vkCmdTraceRaysKHR(
   //    commands,
   //    raygen_binding_table,
   //    miss_binding_table,
   //    hit_binding_table,
   //    nullptr,
   //    dispatch_size.x,
   //    dispatch_size.y,
   //    1);
}

void CommandBuffer::execute_commands() {
    // const auto info = VkGeneratedCommandsInfoNV{
    //     .sType = VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_NV,
    //     .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    //     .pipeline = {},
    //     .indirectCommandsLayout = {},
    //     .streamCount = ,
    //     .pStreams = ,
    //     .sequencesCount = ,
    //     .preprocessBuffer = ,
    //     .preprocessOffset = ,
    //     .preprocessSize = ,
    //     .sequencesCountBuffer = ,
    //     .sequencesCountOffset = ,
    //     .sequencesIndexBuffer = ,
    //     .sequencesIndexOffset = 
    // };
    // vkCmdExecuteGeneratedCommandsNV(commands, VK_FALSE, &info);
}

void CommandBuffer::bind_pipeline(const ComputePipelineHandle& pipeline) {
    current_bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;

    save_pipeline_layout_info(*pipeline);

    vkCmdBindPipeline(commands, current_bind_point, pipeline->pipeline);

    are_bindings_dirty = true;
}

void CommandBuffer::bind_pipeline(const GraphicsPipelineHandle& pipeline) {
    current_bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;

    save_pipeline_layout_info(*pipeline);

    auto& cache = backend->get_pipeline_cache();

    VkPipeline vk_pipeline;
    if(current_render_pass == VK_NULL_HANDLE) {
        vk_pipeline = cache.get_pipeline_for_dynamic_rendering(
            pipeline,
            bound_color_attachment_formats,
            bound_depth_attachment_format,
            bound_view_mask,
            using_fragment_shading_rate_attachment);
    } else {
        vk_pipeline = cache.get_pipeline(pipeline, current_render_pass, current_subpass);
    }

    vkCmdBindPipeline(commands, current_bind_point, vk_pipeline);

    are_bindings_dirty = true;
}

void CommandBuffer::bind_pipeline(const RayTracingPipelineHandle pipeline) {
    current_bind_point = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;

    save_pipeline_layout_info(*pipeline);

    // We compile RT PSOs ahead of time because we can
    vkCmdBindPipeline(commands, current_bind_point, pipeline->pipeline);

    are_bindings_dirty = true;
}

void CommandBuffer::set_push_constant(const uint32_t index, const uint32_t data) {
    push_constants[index] = data;

    are_bindings_dirty = true;
}

void CommandBuffer::set_push_constant(const uint32_t index, const float data) {
    set_push_constant(index, glm::floatBitsToUint(data));
}

void CommandBuffer::bind_buffer_reference(const uint32_t index, const BufferHandle buffer_handle) {
    if(buffer_handle->address == 0) {
        throw std::runtime_error{"Buffer was not created with a device address! Is it a uniform buffer?"};
    }

    set_push_constant(index, buffer_handle->address.low_bits());
    set_push_constant(index + 1, buffer_handle->address.high_bits());
}

void CommandBuffer::bind_descriptor_set(const uint32_t set_index, const DescriptorSet& set) {
    return bind_descriptor_set(set_index, set.descriptor_set);
}

void CommandBuffer::bind_descriptor_set(const uint32_t set_index, const VkDescriptorSet set) {
    descriptor_sets[set_index] = set;

    are_bindings_dirty = true;
}

void CommandBuffer::clear_descriptor_set(const uint32_t set_index) {
    descriptor_sets[set_index] = VK_NULL_HANDLE;

    are_bindings_dirty = true;
}

void CommandBuffer::dispatch(const uint32_t width, const uint32_t height, const uint32_t depth) {
    commit_bindings();

    vkCmdDispatch(commands, width, height, depth);
}

void CommandBuffer::dispatch_indirect(const BufferHandle indirect_buffer) {
    commit_bindings();

    vkCmdDispatchIndirect(commands, indirect_buffer->buffer, 0);
}

void CommandBuffer::copy_buffer_to_buffer(
    const BufferHandle dst, const uint32_t dst_offset, const BufferHandle src, const uint32_t src_offset
) const {
    const auto region = VkBufferCopy2{
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .srcOffset = src_offset,
        .dstOffset = dst_offset,
        .size = src->create_info.size - src_offset
    };

    const auto copy_info = VkCopyBufferInfo2{
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .srcBuffer = src->buffer,
        .dstBuffer = dst->buffer,
        .regionCount = 1,
        .pRegions = &region
    };

    vkCmdCopyBuffer2(commands, &copy_info);
}

void CommandBuffer::copy_image_to_image(const TextureHandle src, const TextureHandle dst) const {
    const auto region = VkImageCopy2{
        .sType = VK_STRUCTURE_TYPE_IMAGE_COPY_2,
        .srcSubresource = {
            .aspectMask = static_cast<VkImageAspectFlags>(is_depth_format(src->create_info.format)
                ? VK_IMAGE_ASPECT_DEPTH_BIT
                : VK_IMAGE_ASPECT_COLOR_BIT),
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .srcOffset = {},
        .dstSubresource = {
            .aspectMask = static_cast<VkImageAspectFlags>(is_depth_format(dst->create_info.format)
                ? VK_IMAGE_ASPECT_DEPTH_BIT
                : VK_IMAGE_ASPECT_COLOR_BIT),
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .dstOffset = {},
        .extent = src->create_info.extent,
    };

    const auto copy_info = VkCopyImageInfo2{
        .sType = VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2,
        .srcImage = src->image,
        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .dstImage = dst->image,
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount = 1,
        .pRegions = &region
    };
    vkCmdCopyImage2(commands, &copy_info);
}

void CommandBuffer::reset_event(const VkEvent event, const VkPipelineStageFlags stages) const {
    vkCmdResetEvent2(commands, event, stages);
}

void CommandBuffer::set_event(const VkEvent event, const std::vector<BufferBarrier>& buffers) {
    auto buffer_barriers = std::vector<VkBufferMemoryBarrier2>{};
    buffer_barriers.reserve(buffers.size());
    for(const auto& buffer_barrier : buffers) {
        const auto barrier = VkBufferMemoryBarrier2{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask = buffer_barrier.src.stage,
            .srcAccessMask = buffer_barrier.src.access,
            .dstStageMask = buffer_barrier.dst.stage,
            .dstAccessMask = buffer_barrier.dst.access,
            .buffer = buffer_barrier.buffer->buffer,
            .offset = buffer_barrier.offset,
            .size = buffer_barrier.size
        };
        buffer_barriers.emplace_back(barrier);
    }

    const auto dependency = VkDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = static_cast<uint32_t>(buffer_barriers.size()),
        .pBufferMemoryBarriers = buffer_barriers.data()
    };
    vkCmdSetEvent2(commands, event, &dependency);

    event_buffer_barriers.emplace(event, std::move(buffer_barriers));
}

void CommandBuffer::wait_event(const VkEvent event) {
    const auto& buffer_barriers = event_buffer_barriers.at(event);

    const auto dependency = VkDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = static_cast<uint32_t>(buffer_barriers.size()),
        .pBufferMemoryBarriers = buffer_barriers.data()
    };
    vkCmdWaitEvents2(commands, 1, &event, &dependency);

    event_buffer_barriers.erase(event);
}

void CommandBuffer::begin_label(const std::string& event_name) const {
    logger->trace("[{}]: begin_label", event_name);
    if(vkCmdBeginDebugUtilsLabelEXT == nullptr) {
        return;
    }

    const auto label = VkDebugUtilsLabelEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pLabelName = event_name.c_str()
    };

    vkCmdBeginDebugUtilsLabelEXT(commands, &label);
}

void CommandBuffer::end_label() const {
    logger->trace("end_label");
    if(vkCmdEndDebugUtilsLabelEXT == nullptr) {
        return;
    }

    vkCmdEndDebugUtilsLabelEXT(commands);
}

void CommandBuffer::end() const {
    vkEndCommandBuffer(commands);
}

void CommandBuffer::save_pipeline_layout_info(const PipelineBase& pipeline) {
    current_pipeline_layout = pipeline.layout;

    num_push_constants_in_current_pipeline = pipeline.num_push_constants;
    push_constant_shader_stages = pipeline.push_constant_stages;

    num_descriptor_sets_in_current_pipeline = static_cast<uint32_t>(pipeline.descriptor_sets.size());
}

void CommandBuffer::bind_index_buffer(const BufferHandle buffer, const VkIndexType index_type) const {
    vkCmdBindIndexBuffer(commands, buffer->buffer, 0, index_type);
}

void CommandBuffer::commit_bindings() {
    if(!are_bindings_dirty) {
        return;
    }

    if(num_push_constants_in_current_pipeline > 0) {
        vkCmdPushConstants(
            commands,
            current_pipeline_layout,
            push_constant_shader_stages,
            0,
            static_cast<uint32_t>(num_push_constants_in_current_pipeline * sizeof(uint32_t)),
            push_constants.data()
        );
    }

    for(uint32_t i = 0; i < descriptor_sets.size(); i++) {
        if(descriptor_sets[i] != VK_NULL_HANDLE && num_descriptor_sets_in_current_pipeline >= i) {
            vkCmdBindDescriptorSets(
                commands,
                current_bind_point,
                current_pipeline_layout,
                i,
                1,
                &descriptor_sets[i],
                0,
                nullptr
            );
        }
    }

    are_bindings_dirty = false;
}

VkCommandBuffer CommandBuffer::get_vk_commands() const {
    return commands;
}

VkRenderPass CommandBuffer::get_current_renderpass() const {
    return current_render_pass;
}

uint32_t CommandBuffer::get_current_subpass() const {
    return current_subpass;
}

RenderBackend& CommandBuffer::get_backend() const {
    return *backend;
}

#if defined(TRACY_ENABLE)
tracy::VkCtx* CommandBuffer::get_tracy_context() const {
    return backend->get_tracy_context();
}
#endif
