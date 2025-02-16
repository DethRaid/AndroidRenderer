#version 460

layout(location = 0) in mediump vec4 sh_in;

layout(location = 0) out mediump vec4 sh_out;

layout(push_constant) uniform Constants {
    uint resolution_x;
    uint resolution_y;
    uint num_cascades;
};

void main() {
    sh_out = sh_in;
}
