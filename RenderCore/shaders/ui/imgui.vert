#version 460

layout(location = 0) in vec2 position_in;
layout(location = 1) in vec2 texcoord_in;
layout(location = 2) in lowp vec4 color_in;

layout(location = 0) out mediump vec2 texcoord_out;
layout(location = 1) out lowp vec4 color_out;

layout(push_constant) uniform Constants {
    uvec2 render_resolution;
    uint has_texture;
};

void main() {
    gl_Position = vec4((position_in / vec2(render_resolution)) * 2.f - 1.f, 0, 1);

    texcoord_out = texcoord_in;
    color_out = color_in;
}
