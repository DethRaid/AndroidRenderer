#version 460

#extension GL_GOOGLE_include_directive : enable

#include "shared/primitive_data.hpp"
#include "shared/view_data.hpp"

layout(location = 0) in vec3 position_in;

layout(location = 0) out vec3 position_out;
layout(location = 1) out vec3 view_direction;

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(set = 1, binding = 0) buffer PrimitiveDataBuffer {
    PrimitiveDataGPU primitive_datas[];
};
layout(set = 1, binding = 1) uniform ViewUniformBuffer {
    ViewDataGPU view_data;
};

void main() {
    PrimitiveDataGPU primitive = primitive_datas[gl_InstanceIndex];

}
