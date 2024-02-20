#version 460

#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference_uvec2 : enable

#include "shared/sun_light_constants.hpp"
#include "shared/primitive_data.hpp"
#include "shared/lpv.hpp"

layout(set = 0, binding = 0, std430) uniform LPVCascadesBuffer {
    LPVCascadeMatrices cascade_matrices[4];
} cascade_matrices_buffer;

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer PrimitiveDataBuffer {
    PrimitiveDataGPU primitive_datas[];
};

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer PrimitiveIdBuffer {
    uint primitive_ids[];
};

layout(push_constant) uniform Constants {
    PrimitiveDataBuffer primitive_data_buffer;
    uint primitive_index;
    uint padding0;
    uint cascade_index;
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
    primitive_id = primitive_index;
    PrimitiveDataGPU data = primitive_data_buffer.primitive_datas[primitive_id];

    gl_Position = cascade_matrices_buffer.cascade_matrices[cascade_index].rsm_vp * data.model * vec4(position_in, 1.f);

    normal_out = normalize(mat3(data.model) * normal_in);
    tangent_out = tangent_in;
    texcoord_out = texcoord_in;
    color_out = color_in;
}
