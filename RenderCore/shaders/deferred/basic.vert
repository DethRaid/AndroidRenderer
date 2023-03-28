#version 460

struct PrimitiveDataGPU {
    mat4 model;
};

layout(set = 0, binding = 0) uniform CameraData {
    mat4 view;
    mat4 projection;
} camera_data;

layout(std430, set = 1, binding = 0) readonly buffer PrimitiveDataBuffer {
    PrimitiveDataGPU primitive_datas[];
} primitive_data_buffer;

layout(push_constant) uniform Constants {
    int primitive_id;
} push_constants;

layout(location = 0) in vec3 position_in;
layout(location = 1) in vec3 normal_in;
layout(location = 2) in vec3 tangent_in;
layout(location = 3) in vec2 texcoord_in;
layout(location = 4) in vec4 color_in;

layout(location = 0) out vec3 normal_out;
layout(location = 1) out vec3 tangent_out;
layout(location = 2) out vec2 texcoord_out;
layout(location = 3) out vec4 color_out;

void main() {
    PrimitiveDataGPU data = primitive_data_buffer.primitive_datas[push_constants.primitive_id];

    gl_Position = camera_data.projection * camera_data.view * data.model * vec4(position_in, 1.f);

    normal_out = normalize(mat3(data.model) * normal_in);
    tangent_out = normalize(mat3(data.model) * tangent_in);
    texcoord_out = texcoord_in;
    color_out = color_in;
}
