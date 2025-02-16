#version 460

/**
 * Generates a SH point cloud from a depth and normal image
 */

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#include "shared/prelude.h"
#include "common/spherical_harmonics.glsl"
#include "shared/view_info.hpp"

layout(set = 0, binding = 0) uniform sampler2D normal_target;
layout(set = 0, binding = 1) uniform sampler2D depth_target;

layout(set = 0, binding = 3) uniform ViewUniformBuffer {
    ViewInfo view_info;
};

layout(push_constant) uniform Constants {
    uint resolution_x;
    uint resolution_y;
    uint num_cascades;
};

layout(location = 0) out mediump vec4 sh;

void main() {
    uint x = gl_VertexIndex % resolution_x;
    uint y = gl_VertexIndex / resolution_x;

    if(x >= resolution_x || y >= resolution_y) {
        gl_Position = vec4(0) / 0.f;
        return;
    }

    float depth = texelFetch(depth_target, ivec2(x, y), 0).x;

    vec2 screenspace = vec2(x, y) / vec2(resolution_x, resolution_y);
    vec4 ndc_position = vec4(screenspace * 2.f - 1.f, depth, 1);
    vec4 viewspace_position = view_info.inverse_projection * ndc_position;
    viewspace_position /= viewspace_position.w;

    vec4 worldspace_position = view_info.inverse_view * viewspace_position;

    mediump vec3 normal = texelFetch(normal_target, ivec2(x, y), 0).xyz;
    sh = dir_to_cosine_lobe(normal);

    gl_Position = worldspace_position;
    gl_PointSize = 1;
}
