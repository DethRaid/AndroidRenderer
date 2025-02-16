#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#include "shared/prelude.h"
#include "shared/lpv.hpp"
#include "shared/view_data.hpp"

layout(set = 0, binding = 0) uniform ViewUniformBuffer {
    ViewDataGPU view_info;
};

layout(push_constant) uniform Constants {
    uvec2 vpl_list_buffer;
    float vpl_size;
};

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location = 0) in mediump vec3 color_in[1];
layout(location = 1) in mediump vec3 normal_in[1];

layout(location = 0) out mediump vec3 color_out;
layout(location = 1) out mediump vec3 normal_out;

void main() {
    // Generate a quad that's 32x32 pixels and aligned to the camera

    const vec4 worldspace_position = gl_in[0].gl_Position;

    const vec4 viewspace_position = view_info.view * worldspace_position;
    vec4 ndc_position = view_info.projection * viewspace_position;
    ndc_position /= ndc_position.w;

    const vec2 offset = vpl_size / view_info.render_resolution.xy;

    color_out = color_in[0];
    normal_out = normal_in[0];
    gl_Position = ndc_position + vec4( offset.x, -offset.y, 0, 0);
    EmitVertex();
    
    color_out = color_in[0];
    normal_out = normal_in[0];
    gl_Position = ndc_position + vec4(-offset.x, -offset.y, 0, 0);
    EmitVertex();
    
    color_out = color_in[0];
    normal_out = normal_in[0];
    gl_Position = ndc_position + vec4( offset.x,  offset.y, 0, 0);
    EmitVertex();
    
    color_out = color_in[0];
    normal_out = normal_in[0];
    gl_Position = ndc_position + vec4(-offset.x,  offset.y, 0, 0);
    EmitVertex();
}
