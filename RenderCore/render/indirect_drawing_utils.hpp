#pragma once

#include "backend/handles.hpp"

class RenderGraph;

struct IndirectDrawingBuffers {
    BufferHandle commands;
    BufferHandle count;
    BufferHandle primitive_ids;
};

/**
 * \brief Translates a visibility list to a list of indirect draw commands
 *
 * The visibility list should have a 0 if the primitive at that index is not visible, 1 if it is
 *
 * The returned buffers are destroyed at the beginning of the next frame. Do not cache them
 *
 * \param graph Render graph to use to execute operations
 * \param visibility_list List of primitive visibility. Contains one uint per primitive: 1 if it's visible, 0 if not
 * \param primitive_buffer List of PrimitiveDataGPUs
 * \param num_primitives Total number of primitives
 * \param mesh_draw_args_buffer Buffer containing the draw arguments for each mesh
 * \param primitive_type The type of primitive to generate buffers for
 * \return A tuple of the draw commands, draw count, and draw ID -> primitive ID mapping buffers
 */
IndirectDrawingBuffers translate_visibility_list_to_draw_commands(
    RenderGraph& graph, BufferHandle visibility_list, BufferHandle primitive_buffer, uint32_t num_primitives,
    BufferHandle mesh_draw_args_buffer, uint32_t primitive_type
);
