#version 460

layout(location = 0) in mediump vec2 texcoord_in;
layout(location = 1) in lowp vec4 color_in;

layout(location = 0) out vec4 color_out;

void main() {
    color_out = color_in;
}
