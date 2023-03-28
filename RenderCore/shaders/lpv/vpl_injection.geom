#version 460

layout(points) in;
layout(points, max_vertices = 1) out;

layout(location = 0) in vec3 position_in[1];
layout(location = 1) in vec3 color_in[1];
layout(location = 2) in vec3 normal_in[1];

layout(location = 0) out vec3 color_out;
layout(location = 1) out vec3 normal_out;

layout(push_constant) uniform Constants {
    int cascade_index;
} push_constants;

void main() {
    vec3 position = position_in[0];
    gl_Layer = int(position.z * 32.f);

    // Adjust position to be in the correct part of the cascade
    position.x += push_constants.cascade_index;
    position.x /= 4.0;  // TODO: NUM_CASCADES as push constant

    gl_Position = vec4(position.xy * 2.f - 1.f, 0.f, 1.f);
    color_out = color_in[0];
    normal_out = normal_in[0];

    EmitVertex();
}
