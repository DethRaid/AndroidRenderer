#version 460

#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#include "shared/sun_light_constants.hpp"
#include "shared/lpv.hpp"

struct PrimitiveDataGPU {
    mat4 model;
};

layout(set = 0, binding = 0, std430) uniform LPVCascadesBuffer {
    LPVCascadeMatrices cascade_matrices[4];
} cascade_matrices_buffer;

layout(std430, set = 0, binding = 1) readonly buffer PrimitiveDataBuffer {
    PrimitiveDataGPU primitive_datas[];
} primitive_data_buffer;

layout(push_constant) uniform Constants {
    int cascade_index;
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

    gl_Position = cascade_matrices_buffer.cascade_matrices[push_constants.cascade_index].rsm_vp * data.model * vec4(position_in, 1.f);

    normal_out = normalize(mat3(data.model) * normal_in);
    tangent_out = tangent_in;
    texcoord_out = texcoord_in;
    color_out = color_in;
}
