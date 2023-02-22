#pragma once

#include <cstdint>
#include <stdexcept>

#include <spdlog/spdlog.h>

#include "render/backend/handles.hpp"
#include "render/backend/render_backend.hpp"
#include "console/cvars.hpp"
#include "core/system_interface.hpp"

static AutoCVar_Int cvar_scatter_buffer_size = {
    "r.PrimitiveUpload.BatchSize",
    "Number of primitives to upload in one batch", 1024
};

template <typename DataType>
class ScatterUploadBuffer {
public:
    explicit ScatterUploadBuffer(RenderBackend& backend_in);

    void add_data(uint32_t destination_index, DataType data);

    void flush_to_buffer(CommandBuffer& commands, BufferHandle destination_buffer);

    uint32_t get_size() const;

    bool is_full() const;

private:
    RenderBackend* backend;

    uint32_t scatter_buffer_count = 0;

    BufferHandle scatter_indices = BufferHandle::None;
    BufferHandle scatter_data = BufferHandle::None;

    static ComputeShader scatter_shader;
};

template <typename DataType>
ComputeShader ScatterUploadBuffer<DataType>::scatter_shader;

template <typename DataType>
ScatterUploadBuffer<DataType>::ScatterUploadBuffer(RenderBackend& backend_in) : backend{&backend_in} {
    if (scatter_shader.pipeline == VK_NULL_HANDLE) {
        const auto shader_maybe = SystemInterface::get().load_file("shaders/scatter_upload.comp.spv");

        if (!shader_maybe || shader_maybe->empty()) {
            spdlog::error("Could not load compute shader shaders/scatter_upload.comp.spv");
            throw std::runtime_error{"Could not load compute shader shaders/scatter_upload.comp.spv"};
        }

        scatter_shader = *backend->create_compute_shader("shaders/scatter_upload.comp.spv", *shader_maybe);
    }
}

template <typename DataType>
void ScatterUploadBuffer<DataType>::add_data(uint32_t destination_index, DataType data) {
    auto& allocator = backend->get_global_allocator();

    if (scatter_indices == BufferHandle::None) {
        scatter_indices = allocator.create_buffer(
            "Primitive scatter indices", cvar_scatter_buffer_size.Get(),
            BufferUsage::StagingBuffer
        );
    }

    auto& scatter_indices_actual = allocator.get_buffer(scatter_indices);
    static_cast<uint32_t*>(scatter_indices_actual.allocation_info
                                                 .pMappedData)[scatter_buffer_count] = destination_index;

    if (scatter_data == BufferHandle::None) {
        scatter_data = allocator.create_buffer(
            "Primitive scatter data",
            cvar_scatter_buffer_size.Get() * sizeof(DataType),
            BufferUsage::StagingBuffer
        );
    }

    auto& scatter_data_actual = allocator.get_buffer(scatter_data);
    static_cast<DataType*>(scatter_data_actual.allocation_info.pMappedData)[scatter_buffer_count] = data;

    scatter_buffer_count++;
}

template <typename DataType>
uint32_t ScatterUploadBuffer<DataType>::get_size() const {
    return scatter_buffer_count;
}

template <typename DataType>
bool ScatterUploadBuffer<DataType>::is_full() const {
    return scatter_buffer_count >= cvar_scatter_buffer_size.Get();
}

template <typename DataType>
void ScatterUploadBuffer<DataType>::flush_to_buffer(CommandBuffer& commands, BufferHandle destination_buffer) {
    if (scatter_indices == BufferHandle::None ||
        scatter_data == BufferHandle::None) {
        return;
    }

    auto& resources = backend->get_global_allocator();

    commands.flush_buffer(scatter_indices);
    commands.flush_buffer(scatter_data);

    const auto& scatter_indices_actual = resources.get_buffer(scatter_indices);
    const auto& scatter_data_actual = resources.get_buffer(scatter_data);
    const auto& destination_buffer_actual = resources.get_buffer(destination_buffer);

    auto scatter_indices_info = VkDescriptorBufferInfo{
        .buffer = scatter_indices_actual.buffer,
        .offset = 0,
        .range = scatter_indices_actual.create_info.size,
    };
    auto scatter_data_info = VkDescriptorBufferInfo{
        .buffer = scatter_data_actual.buffer,
        .offset = 0,
        .range = scatter_data_actual.create_info.size,
    };
    auto destination_buffer_info = VkDescriptorBufferInfo{
        .buffer = destination_buffer_actual.buffer,
        .offset = 0,
        .range = destination_buffer_actual.create_info.size,
    };

    auto set = VkDescriptorSet{};
    backend->create_frame_descriptor_builder()
           .bind_buffer(0, &scatter_indices_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
           .bind_buffer(1, &scatter_data_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
           .bind_buffer(2, &destination_buffer_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
           .build(set);

    commands.set_resource_usage(destination_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);

    commands.bind_descriptor_set(0, set);

    commands.set_push_constant(0, scatter_buffer_count);

    commands.bind_shader(scatter_shader);

    commands.dispatch(scatter_buffer_count / 32, 1, 1);

    commands.clear_descriptor_set(0);

    // Free the existing scatter upload buffers. We'll allocate new ones when/if we need them

    resources.destroy_buffer(scatter_indices);
    resources.destroy_buffer(scatter_data);

    scatter_indices = BufferHandle::None;
    scatter_data = BufferHandle::None;

    scatter_buffer_count = 0;
}
