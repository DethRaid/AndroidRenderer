#include "resettable_buffer.hpp"

#include "render_graph.hpp"

void ResettableBuffer::reset(RenderGraph& graph) const {
    graph.add_copy_pass({.name = "Reset buffer", .dst = buffer, .src = initial_value_buffer});
}
