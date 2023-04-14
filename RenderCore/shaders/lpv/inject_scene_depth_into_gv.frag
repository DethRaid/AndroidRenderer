#version 460

layout(location = 0) in mediump vec4 sh_in;

layout(location = 0) out mediump vec4 sh_out;

void main() {
    sh_out = sh_in;
}
