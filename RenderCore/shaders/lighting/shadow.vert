#version 460

#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_multiview : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference_uvec2 : enable

#include "shared/sun_light_constants.hpp"
#include "shared/primitive_data.hpp"

layout(set = 0, binding = 0, std140) uniform ShadowCascadesBuffer {
    SunLightConstants sun_light;
};

layout(set = 0, binding = 1, scalar) readonly buffer PrimitiveDataBuffer {
    PrimitiveDataGPU primitive_datas[];
};

layout(push_constant, scalar) uniform Constants {
    uint primitive_id;
};

layout(location = 0) in vec3 position_in;

void main() {
    PrimitiveDataGPU data = primitive_datas[primitive_id];

    gl_Position = sun_light.cascade_matrices[gl_ViewIndex] * data.model * vec4(position_in, 1.f);
}
