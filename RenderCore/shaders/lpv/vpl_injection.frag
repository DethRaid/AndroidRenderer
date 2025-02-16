#version 460

#extension GL_GOOGLE_include_directive : enable

#include "common/spherical_harmonics.glsl"

layout(location = 0) in mediump vec3 color_in;
layout(location = 1) in mediump vec3 normal_in;

layout(location = 0) out mediump vec4 red;
layout(location = 1) out mediump vec4 green;
layout(location = 2) out mediump vec4 blue;

void main() {
    if(length(normal_in) < 1 || length(color_in) == 0) {
        discard;
    }

    mediump vec3 scaled_color = color_in * (32 * 32) / (512 * 512);

    mediump vec3 normal = normal_in;
    // normal.z *= -1;

    red = dir_to_sh(normal) * color_in.r / PI;
    green = dir_to_sh(normal) * color_in.g / PI;
    blue = dir_to_sh(normal) * color_in.b / PI;
}
