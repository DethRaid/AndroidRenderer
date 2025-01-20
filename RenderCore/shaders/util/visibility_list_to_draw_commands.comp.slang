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

[[vk::push_constant]]
cbuffer Constants {
    PrimitiveDataGPU *primitive_datas;
    uint *visibility_array;
    DrawCommand *meshes;
    DrawCommand *draw_commands;
    DrawCountBuffer *draw_count_buffer;
    uint *primitive_ids;
    uint num_primitives;
};

[shader("compute")]
[numthreads(96, 1, 1)]
void main() {
    if (gl_GlobalInvocationID.x >= num_primitives) {
        return;
    }

    const uint primitive_id = gl_GlobalInvocationID.x;

    // uint *visibility_array = Ptr<uint>(visible_ptr);
    const uint visible = visibility_array[primitive_id];

    //debugPrintfEXT("primitive_id=%d visible=%d", primitive_id, visible);

    if (visible != 0) {
        //PrimitiveDataGPU *primitive_datas = Ptr<PrimitiveDataGPU>(primitive_datas_ptr);
        PrimitiveDataGPU primitive_data = primitive_datas[primitive_id];

        //DrawCountBuffer *draw_count_buffer = Ptr<DrawCountBuffer>(draw_count_buffer_ptr);

        uint draw_id;
        if (primitive_data.type == PRIMITIVE_TYPE_SOLID) {
            draw_id = atomicAdd(draw_count_buffer.opaque_draw_count, 1);
        } else {
            atomicAdd(draw_count_buffer.masked_draw_count, 1);
            draw_id = atomicAdd(draw_count_buffer.masked_draw_offset, -1);
        }

       // debugPrintfEXT("draw_id=%d", draw_id);

        // uint *primitive_ids = Ptr<uint>(primitive_ids_ptr);
        primitive_ids[draw_id] = primitive_id;

        // DrawCommand *meshes = Ptr<DrawCommand>(meshes_ptr);
        // DrawCommand *draw_commands = Ptr<DrawCommand>(draw_commands_ptr);
        draw_commands[draw_id] = meshes[primitive_data.mesh_id];
        draw_commands[draw_id].firstInstance = draw_id;
    }
}

