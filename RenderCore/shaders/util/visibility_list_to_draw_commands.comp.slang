#version 460

#include "shared/primitive_data.hpp"

struct DrawCommand {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int  vertexOffset;
    uint firstInstance;
};

struct DrawCountBuffer {
    uint opaque_draw_count;
    uint opaque_draw_offset;
    uint masked_draw_count;
    uint masked_draw_offset;
};

struct Constants {
    [[vk::push_constant]]  PrimitiveDataGPU *primitive_datas;
    [[vk::push_constant]]  uint *visible;
    [[vk::push_constant]]  DrawCommand *meshes;
    [[vk::push_constant]]  DrawCommand *draw_commands;
    [[vk::push_constant]]  DrawCountBuffer *draw_count_buffer;
    [[vk::push_constant]]  uint* primitive_ids;
    [[vk::push_constant]]  uint num_primitives;
} constants;

[shader("compute")]
[numthreads(96, 1, 1)]
void main() {
    if (gl_GlobalInvocationID.x >= constants.num_primitives) {
        return;
    }

    const uint primitive_id = gl_GlobalInvocationID.x;

    const uint visible = constants.visible[primitive_id];

    //debugPrintfEXT("primitive_id=%d visible=%d", primitive_id, visible);

    if (visible != 0) {
        PrimitiveDataGPU primitive_data = constants.primitive_datas[primitive_id];

        uint draw_id;
        if (primitive_data.type == PRIMITIVE_TYPE_SOLID) {
            draw_id = atomicAdd(constants.draw_count_buffer.opaque_draw_count, 1);
        } else {
            atomicAdd(constants.draw_count_buffer.masked_draw_count, 1);
            draw_id = atomicAdd(constants.draw_count_buffer.masked_draw_offset, -1);
        }

       // debugPrintfEXT("draw_id=%d", draw_id);

        constants.primitive_ids[draw_id] = primitive_id;

        constants.draw_commands[draw_id] = constants.meshes[primitive_data.mesh_id];
        constants.draw_commands[draw_id].firstInstance = draw_id;
    }
}

