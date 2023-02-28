#version 460

layout(points) in;
layout(points, max_vertices = 1) out;

layout(location = 0) in vec3 position_in[1];
layout(location = 1) in vec3 color_in[1];
layout(location = 2) in vec3 normal_in[1];

layout(location = 0) out vec3 color_out;
layout(location = 1) out vec3 normal_out;

void main() {
    gl_Layer = int(position_in[0].z * 32.f);
    gl_Position = vec4(position_in[0].xy * 2.f - 1.f, 0.5f, 1.f);
    color_out = color_in[0];
    normal_out = normal_in[0];

    EmitVertex();
}
