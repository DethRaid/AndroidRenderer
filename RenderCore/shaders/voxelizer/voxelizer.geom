#version 460

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in vec3 position_in[3];
layout(location = 1) in vec3 normal_in[3];

layout(location = 0) out vec3 normal_out;

void main() {
    // Somehow dispatch this triangle to the correct cubemap axis I don't even know

    for(uint vertex_index = 0; vertex_index < 3; vertex_index++) {    
        gl_Position = vec4(position_in[vertex_index], 1.f);
        normal_out = normal_in[vertex_index];
        
        EmitVertex();
    }
}
