#version 460

#extension GL_GOOGLE_include_directive : enable

#include "common/spherical_harmonics.glsl"

layout(location = 0) in vec3 color_in;
layout(location = 1) in vec3 normal_in;

layout(location = 0) out vec4 red;
layout(location = 1) out vec4 green;
layout(location = 2) out vec4 blue;

void main() {
    vec3 scaled_color = color_in * (32 * 32) / (1024 * 1024);

    red = sh_project_cone(normal_in * scaled_color.r);
    blue = sh_project_cone(normal_in * scaled_color.g);
    green = sh_project_cone(normal_in * scaled_color.b);
}