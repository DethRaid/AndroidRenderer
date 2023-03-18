#version 460

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in vec3 position_in[3];
layout(location = 1) in vec3 normal_in[3];

layout(location = 0) out vec3 position_out;
layout(location = 1) out vec3 normal_out;

void main() {
    vec3 min_bounds = min(min(position_in[0], position_in[1]), position_in[2]);
    vec3 max_bounds = max(max(position_in[0], position_in[1]), position_in[2]);
    vec3 bounds_size = max_bounds - min_bounds;

    for(uint vertex_index = 0; vertex_index < 3; vertex_index++) {
        vec3 output_position = position_in[vertex_index];

        // The triangle is flattest in X
        if(bounds_size.x < bounds_size.y && bounds_size.x < bounds_size.z) {
            gl_Position = vec4(output_position.yz, 0.f, 1.f);
        }
        // The triangle is flattest in Y
        if(bounds_size.y < bounds_size.x && bounds_size.y < bounds_size.x) {
            gl_Position = vec4(output_position.xz, 0.f, 1.f);
        }
        // The triangle is flattest in Z
        if(bounds_size.z < bounds_size.x && bounds_size.z < bounds_size.y) {
            gl_Position = vec4(output_position.xy, 0.f, 1.f);
        }

        position_out = position_in[vertex_index];
        normal_out = normal_in[vertex_index];
        
        EmitVertex();
    }
}
