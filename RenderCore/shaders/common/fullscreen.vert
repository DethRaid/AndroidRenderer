#version 460 core

out gl_PerVertex {
    vec4 gl_Position;
};

layout(location = 0) out vec2 texcoord;

void main() {
    if(gl_VertexIndex == 0) {
        gl_Position = vec4(-1.f, -1.f, 0.f, 1);
        texcoord = vec2(0.f, 1.f);

    } else if(gl_VertexIndex == 1) {
        gl_Position = vec4(-1.f, 3.f, 0.f, 1);
        texcoord = vec2(0.f, -1.f);

    } else if(gl_VertexIndex == 2) {
        gl_Position = vec4(3.f, -1.f, 0.f, 1);
        texcoord = vec2(2.f, 1.f);

    } else {
        gl_Position = vec4(0.f);
        texcoord = vec2(0.f, 0.f);
    }
}
