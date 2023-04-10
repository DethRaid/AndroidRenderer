#version 460

layout(location = 0) in vec4 sh_in;

layout(location = 1) out vec4 sh_out;

void main() {
    sh_out = sh_in;
}
