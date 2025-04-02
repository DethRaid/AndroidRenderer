#version 460

layout(location = 0) in mediump vec3 color_in;
layout(location = 1) in mediump vec3 normal_in;

layout(location = 0) out mediump vec4 color_out;

void main() {
    color_out = vec4(color_in, 1.f);
}
