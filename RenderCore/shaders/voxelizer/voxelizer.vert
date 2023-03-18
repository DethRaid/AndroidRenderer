#version 460

#extension GL_GOOGLE_include_directive : enable

struct PrimitiveDataGPU {
    mat4 model;
};

layout(set = 0, binding = 0, std140) uniform VoxelBoundsBuffer {
    mat4 world_to_bounds;
};

layout(std430, set = 1, binding = 0) readonly buffer PrimitiveDataBuffer {
    PrimitiveDataGPU primitive_datas[];
} primitive_data_buffer;

layout(push_constant) uniform Constants {
    int primitive_id;
} push_constants;

layout(location = 0) in vec3 position_in;
layout(location = 1) in vec3 normal_in;

layout(location = 0) out vec3 position_out;
layout(location = 1) out vec3 normal_out;

// Transform the vertex into voxel space

void main() {
    PrimitiveDataGPU data = primitive_data_buffer.primitive_datas[push_constants.primitive_id];

    gl_Position = world_to_bounds * data.model * vec4(position_in, 1.f);

    position_out = gl_Position.xyz / gl_Position.w;
    normal_out = normal_in;
}
