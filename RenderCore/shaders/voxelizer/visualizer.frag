#version 460

#extension GL_GOOGLE_include_directive : enable

#include "shared/primitive_data.hpp"
#include "shared/view_data.hpp"

layout(set = 1, binding = 0) uniform ViewUniformBuffer {
    ViewDataGPU view_data;
};
layout(set = 1, binding = 1) buffer PrimitiveDataBuffer {
    PrimitiveDataGPU primitive_datas[];
};

layout(location = 0) in vec3 texcoord_in;
layout(location = 1) in vec3 uvspace_view_direction_in;
layout(location = 2) in flat uint primitive_index_in;

layout(location = 0) out vec4 color;

void main() {
    color = vec4(texcoord_in, 1);
}
