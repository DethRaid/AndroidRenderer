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
 * \param graph
 * \param visibility_list
 * \param primitive_buffer
 * \param num_primitives
 * \param mesh_draw_args_buffer
 * \return A tuple of the draw commands, draw count, and draw ID -> primitive ID mapping buffers
 */
IndirectDrawingBuffers translate_visibility_list_to_draw_commands(
    RenderGraph& graph, BufferHandle visibility_list, BufferHandle primitive_buffer, uint32_t num_primitives,
    BufferHandle mesh_draw_args_buffer
);
