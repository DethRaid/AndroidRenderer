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

StructuredBuffer<PrimitiveDataGPU> primitive_datas;
StructuredBuffer<uint> visibility_array;
StructuredBuffer<DrawCommand> meshes;
RWStructuredBuffer<DrawCommand> draw_commands;
RWStructuredBuffer<DrawCountBuffer> draw_count_buffer;
RWStructuredBuffer<uint> primitive_ids;

[[vk::push_constant]]
cbuffer Constants {
    uint num_primitives;
};

[require(SPV_KHR_non_semantic_info)]
[shader("compute")]
[numthreads(96, 1, 1)]
void main(uint thread_id: SV_DispatchThreadID) {
    if (thread_id >= num_primitives) {
        return;
    }

    const uint primitive_id = thread_id;

    const uint visible = visibility_array[primitive_id];

    if (visible != 0) {
        PrimitiveDataGPU primitive_data = primitive_datas[primitive_id];

        uint draw_id;
        if (primitive_data.type == PRIMITIVE_TYPE_SOLID) {
            InterlockedAdd(draw_count_buffer[0].opaque_draw_count, 1, draw_id);
        } else {
            InterlockedAdd(draw_count_buffer[0].masked_draw_count, 1);
            InterlockedAdd(draw_count_buffer[0].masked_draw_offset, -1, draw_id);
        }

        primitive_ids[draw_id] = primitive_id;

        draw_commands[draw_id] = meshes[primitive_data.mesh_id];
        draw_commands[draw_id].firstInstance = draw_id;
    }
}

