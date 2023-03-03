#version 460

layout(location = 0) in vec3 normal_in;

layout(location = 0) out vec4 data_out;

void main() {
    // TODO: Sample the appropriate textures and apply the appropriate transformations

    data_out = vec4(normal_in, 1.f);
}
