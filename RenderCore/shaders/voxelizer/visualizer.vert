#version 460

#extension GL_GOOGLE_include_directive : enable

#include "shared/primitive_data.hpp"
#include "shared/view_data.hpp"

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(set = 1, binding = 0) uniform ViewUniformBuffer {
    ViewDataGPU view_data;
};
layout(set = 1, binding = 1) buffer PrimitiveDataBuffer {
    PrimitiveDataGPU primitive_datas[];
};

layout(location = 0) in vec3 position_in;

layout(location = 0) out vec3 texcoord_out;
layout(location = 1) out vec3 uvspace_view_direction_out;
layout(location = 2) out uint primitive_index_out;

void main() {
    primitive_index_out = gl_InstanceIndex;
    
    PrimitiveDataGPU primitive = primitive_datas[gl_InstanceIndex];

    const vec4 worldspace_position = primitive.model * vec4(position_in, 1);
    const vec4 position = view_data.projection * view_data.view * worldspace_position;

    gl_Position = position;

    // Vertex position is from -1 to 1
    texcoord_out = position_in * 0.5 + 0.5;
}
