#include "indirect_drawing_utils.hpp"

#include <shared/prelude.h>

#include "backend/pipeline_cache.hpp"
#include "backend/render_backend.hpp"
#include "render/backend/render_graph.hpp"

static ComputePipelineHandle init_count_buffer_pipeline = nullptr;

static ComputePipelineHandle visibility_list_to_draw_commands = nullptr;

IndirectDrawingBuffers translate_visibility_list_to_draw_commands(
    RenderGraph& graph, const BufferHandle visibility_list, const BufferHandle primitive_buffer,
    const uint32_t num_primitives, const BufferHandle mesh_draw_args_buffer, const uint32_t primitive_type
) {
    ZoneScoped;

    auto& backend = RenderBackend::get();

    auto& pipeline_cache = backend.get_pipeline_cache();

    if(!init_count_buffer_pipeline) {
        init_count_buffer_pipeline = pipeline_cache.create_pipeline(
            "shaders/util/init_count_buffer.comp.spv");
    }
    if(!visibility_list_to_draw_commands) {
        visibility_list_to_draw_commands = pipeline_cache.create_pipeline(
            "shaders/util/visibility_list_to_draw_commands.comp.spv");
    }

    auto& allocator = backend.get_global_allocator();
    const auto buffers = IndirectDrawingBuffers{
        .commands = allocator.create_buffer(
            "Draw commands",
            sizeof(VkDrawIndexedIndirectCommand) * num_primitives,
            BufferUsage::IndirectBuffer
        ),
        .count = allocator.create_buffer(
            "draw_count",
            sizeof(uint32_t),
            BufferUsage::IndirectBuffer),
        .primitive_ids = allocator.create_buffer(
            "Primitive ID",
            sizeof(uint32_t) * num_primitives,
            BufferUsage::VertexBuffer
        )
    };

    auto& descriptor_allocator = backend.get_transient_descriptor_allocator();

    const auto init_set = descriptor_allocator.build_set(init_count_buffer_pipeline, 0)
                                             .bind(buffers.count)
                                             .build();
    graph.add_compute_dispatch<uint>(
        {
            .name = "Init dual bump point",
            .descriptor_sets = {init_set},
            .num_workgroups = {1, 1, 1},
            .compute_shader = init_count_buffer_pipeline
        });

    const auto tvl_set = descriptor_allocator.build_set(visibility_list_to_draw_commands, 0)
                                             .bind(primitive_buffer)
                                             .bind(visibility_list)
                                             .bind(mesh_draw_args_buffer)
                                             .bind(buffers.commands)
                                             .bind(buffers.count)
                                             .bind(buffers.primitive_ids)
                                             .build();
    graph.add_compute_dispatch<glm::uvec2>(
        {
            .name = "Translate visibility list",
            .descriptor_sets = {tvl_set},
            .push_constants = glm::uvec2{num_primitives, primitive_type},
            .num_workgroups = {(num_primitives + 95) / 96, 1, 1},
            .compute_shader = visibility_list_to_draw_commands
        }
    );

    return buffers;
}
