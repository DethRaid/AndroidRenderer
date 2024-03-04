#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_16bit_storage : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "shared/primitive_data.hpp"
#include "shared/view_data.hpp"

layout(set = 0, binding = 0) uniform ViewUniformBuffer {
    ViewDataGPU view_data;
};
layout(set = 0, binding = 1) buffer PrimitiveDataBuffer {
    PrimitiveDataGPU primitive_datas[];
};

layout(location = 0) in vec3 position_in;

layout(location = 0) out vec3 texcoord_out;
layout(location = 1) out vec3 uvspace_view_direction_out;
layout(location = 2) out uint primitive_index_out;

void main() {
    primitive_index_out = gl_InstanceIndex;
    
    PrimitiveDataGPU primitive = primitive_datas[primitive_index_out];

    const bvec3 is_negative = lessThan(position_in, vec3(0.f, 0.f, 0.f));
    const vec3 scale = abs(mix(primitive.bounds_max.xyz, primitive.bounds_min_and_radius.xyz, is_negative));

    const vec3 scaled_position = position_in * scale;
    const vec4 worldspace_position = primitive.model * vec4(scaled_position, 1);
    const vec4 viewspace_position = view_data.view * worldspace_position;
    const vec4 position = view_data.projection * viewspace_position;

    const vec3 view_location = vec3(view_data.view[0][3], view_data.view[1][3], view_data.view[2][3]);

    const vec3 viewspace_view_vector = normalize(worldspace_position.xyz - view_location);
    const vec3 modelspace_view_vector = (primitive.inverse_model * vec4(viewspace_view_vector, 0.f)).xyz / scale;
    const vec3 normalized_view_vector = modelspace_view_vector / vec3(primitive.voxel_size_xy, primitive.voxel_size_zw.x);
    uvspace_view_direction_out = normalized_view_vector * 0.5f + 0.5f;

    // uvspace_view_direction_out = uvspace_view_direction_out;

    gl_Position = position;

    // Vertex position is from -1 to 1
    texcoord_out = position_in * 0.5 + 0.5;
}
