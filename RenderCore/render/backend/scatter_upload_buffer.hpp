#pragma once

#include <cstdint>
#include <stdexcept>

#include <spdlog/spdlog.h>

#include "pipeline_cache.hpp"
#include "render/backend/handles.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/render_graph.hpp"
#include "console/cvars.hpp"
#include "core/system_interface.hpp"

constexpr inline auto scatter_buffer_size = 1024u;

template <typename DataType>
class ScatterUploadBuffer {
public:
    explicit ScatterUploadBuffer();

    void add_data(uint32_t destination_index, DataType data);

    void flush_to_buffer(RenderGraph& graph, BufferHandle destination_buffer);

    uint32_t get_size() const;

    bool is_full() const;

private:
    uint32_t scatter_buffer_count = 0;

    BufferHandle scatter_indices = {};
    BufferHandle scatter_data = {};

    static ComputePipelineHandle scatter_shader;
};

template <typename DataType>
ComputePipelineHandle ScatterUploadBuffer<DataType>::scatter_shader;

template <typename DataType>
ScatterUploadBuffer<DataType>::ScatterUploadBuffer() {
    auto& backend = RenderBackend::get();
    if (!scatter_shader.is_valid()) {
        auto& pipeline_cache = backend.get_pipeline_cache();
        scatter_shader = pipeline_cache.create_pipeline("shaders/scatter_upload.comp.spv");
    }
}

template <typename DataType>
void ScatterUploadBuffer<DataType>::add_data(const uint32_t destination_index, DataType data) {
    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();

    if (!scatter_indices) {
        scatter_indices = allocator.create_buffer(
            "Primitive scatter indices", scatter_buffer_size,
            BufferUsage::StagingBuffer
        );
    }

    static_cast<uint32_t*>(scatter_indices->allocation_info.pMappedData)[scatter_buffer_count] = destination_index;

    if (!scatter_data) {
        scatter_data = allocator.create_buffer(
            "Primitive scatter data",
            scatter_buffer_size * sizeof(DataType),
            BufferUsage::StagingBuffer
        );
    }

    static_cast<DataType*>(scatter_data->allocation_info.pMappedData)[scatter_buffer_count] = data;

    scatter_buffer_count++;
}

template <typename DataType>
uint32_t ScatterUploadBuffer<DataType>::get_size() const {
    return scatter_buffer_count;
}

template <typename DataType>
bool ScatterUploadBuffer<DataType>::is_full() const {
    return scatter_buffer_count >= scatter_buffer_size;
}

template <typename DataType>
void ScatterUploadBuffer<DataType>::flush_to_buffer(RenderGraph& graph, BufferHandle destination_buffer) {
    if (!scatter_indices || !scatter_data) {
        return;
    }

    auto& backend = RenderBackend::get();
    auto& resources = backend.get_global_allocator();

    graph.add_pass(
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
                const auto data_size = static_cast<uint32_t>(sizeof(DataType));
                commands.set_push_constant(7, data_size);

                commands.bind_pipeline(scatter_shader);

                // Add 1 because integer division is fun
                commands.dispatch(scatter_buffer_count / 32 + 1, 1, 1);
                
                // Free the existing scatter upload buffers. We'll allocate new ones when/if we need them

                resources.destroy_buffer(scatter_indices);
                resources.destroy_buffer(scatter_data);

                scatter_indices = {};
                scatter_data = {};
            }
        }
    );

    scatter_buffer_count = 0;
}
