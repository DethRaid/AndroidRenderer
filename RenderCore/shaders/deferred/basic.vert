#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference_uvec2 : enable
#extension GL_ARB_shader_draw_parameters : enable

#include "shared/primitive_data.hpp"
#include "shared/view_data.hpp"

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer PrimitiveDataBuffer {
    PrimitiveDataGPU primitive_datas[];
};

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer PrimitiveIdBuffer {
    uint primitive_ids[];
};

layout(set = 0, binding = 0) uniform CameraData {
    ViewDataGPU camera_data;
};

layout(push_constant) uniform Constants {
    PrimitiveDataBuffer primitive_data_buffer;
    PrimitiveIdBuffer primitive_id_buffer;
};

layout(location = 0) in vec3 position_in;
layout(location = 1) in vec3 normal_in;
layout(location = 2) in vec3 tangent_in;
layout(location = 3) in vec2 texcoord_in;
layout(location = 4) in vec4 color_in;

layout(location = 0) out mediump vec3 normal_out;
layout(location = 1) out mediump vec3 tangent_out;
layout(location = 2) out vec2 texcoord_out;
layout(location = 3) out mediump vec4 color_out;
layout(location = 4) flat out uint primitive_id;

void main() {
    primitive_id = primitive_id_buffer.primitive_ids[gl_DrawID];
    PrimitiveDataGPU data = primitive_data_buffer.primitive_datas[primitive_id];

    gl_Position = camera_data.projection * camera_data.view * data.model * vec4(position_in, 1.f);

    normal_out = normalize(mat3(data.model) * normal_in);
    tangent_out = normalize(mat3(data.model) * tangent_in);
    texcoord_out = texcoord_in;
    color_out = color_in;
}
