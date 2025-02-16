#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference_uvec2 : enable

#include "shared/primitive_data.hpp"

layout(set = 0, binding = 1) readonly buffer PrimitiveDataBuffer {
    PrimitiveDataGPU primitive_datas[];
};
layout(set = 0, binding = 2) uniform FrustumMatricesBuffer {
    mat4 world_to_bounds;
};

layout(push_constant) uniform Constants {
    uint primitive_id;
};

layout(location = 0) in vec3 position_in;
layout(location = 3) in vec2 texcoord_in;
layout(location = 4) in mediump vec4 color_in;

layout(location = 0) out vec3 position_out;
layout(location = 1) out mediump vec2 texcoord_out;
layout(location = 2) out mediump vec4 color_out;

void main() {
    PrimitiveDataGPU data = primitive_datas[primitive_id];

    gl_Position = world_to_bounds * data.model * vec4(position_in, 1.f);

    position_out = gl_Position.xyz / gl_Position.w;
    texcoord_out = texcoord_in;
    color_out = color_in;
}
