#version 460

#extension GL_EXT_multiview : enable
#extension GL_GOOGLE_include_directive : enable

#include "shared/sun_light_constants.hpp"

struct PrimitiveDataGPU {
    mat4 model;
};

layout(set = 0, binding = 0, std140) uniform ShadowCascadesBuffer {
    SunLightConstants sun_light;
};

layout(std430, set = 1, binding = 0) readonly buffer PrimitiveDataBuffer {
    PrimitiveDataGPU primitive_datas[];
} primitive_data_buffer;

layout(push_constant) uniform Constants {
    int primitive_id;
} push_constants;

layout(location = 0) in vec3 position_in;


void main() {
    PrimitiveDataGPU data = primitive_data_buffer.primitive_datas[push_constants.primitive_id];

    gl_Position = sun_light.cascade_matrices[gl_ViewIndex] * data.model * vec4(position_in, 1.f);
}
