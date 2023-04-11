#pragma once

#include <cstdint>
#include <stdexcept>

#include <spdlog/spdlog.h>

#include "render/backend/handles.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/render_graph.hpp"
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

    void flush_to_buffer(RenderGraph& graph, BufferHandle destination_buffer);

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
void ScatterUploadBuffer<DataType>::flush_to_buffer(RenderGraph& graph, BufferHandle destination_buffer) {
    if (scatter_indices == BufferHandle::None ||
        scatter_data == BufferHandle::None) {
        return;
    }

    auto& resources = backend->get_global_allocator();

    graph.add_compute_pass(
        ComputePass{
            .name = "Flush scatter buffer",
            .buffers = {
                {scatter_indices, {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT}},
                {scatter_data, {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT}},
                {destination_buffer, {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT}},
            },
            .execute = [&](CommandBuffer& commands) {
                commands.flush_buffer(scatter_indices);
                commands.flush_buffer(scatter_data);
                
                commands.bind_buffer_reference(0, scatter_indices);
                commands.bind_buffer_reference(2, scatter_data);
                commands.bind_buffer_reference(4, destination_buffer);
                commands.set_push_constant(6, scatter_buffer_count);
                commands.set_push_constant(7, static_cast<uint32_t>(sizeof(DataType)));

                commands.bind_shader(scatter_shader);

                // Add 1 because integer division is fun
                commands.dispatch(scatter_buffer_count / 32 + 1, 1, 1);
                
                // Free the existing scatter upload buffers. We'll allocate new ones when/if we need them

                resources.destroy_buffer(scatter_indices);
                resources.destroy_buffer(scatter_data);

                scatter_indices = BufferHandle::None;
                scatter_data = BufferHandle::None;
            }
        }
    );

    scatter_buffer_count = 0;
}
