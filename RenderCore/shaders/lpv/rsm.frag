#version 460
#pragma shader_stage(fragment)

struct BasicPbrMaterialGpu {
    vec4 base_color_tint;
    vec4 emission_factor;
    float metalness_factor;
    float roughness_factor;

    vec2 padding0;
    vec4 padding1;
};

layout(push_constant) uniform Constants {
    int primitive_id;
    int constant1;
    int constant2;
    int constant3;
} push_constants;

layout(set = 1, binding = 0) uniform sampler2D base_color_texture;
layout(set = 1, binding = 1) uniform sampler2D normal_texture;
layout(set = 1, binding = 2) uniform sampler2D data_texture;
layout(set = 1, binding = 3) uniform sampler2D emission_texture;
layout(set = 1, binding = 4) uniform MaterialData {
    BasicPbrMaterialGpu material;
};

layout(location = 0) in vec3 vertex_normal;
layout(location = 1) in vec3 vertex_tangent;
layout(location = 2) in vec2 vertex_texcoord;
layout(location = 3) in vec4 vertex_color;

layout(location = 0) out vec4 rsm_flux;
layout(location = 1) out vec4 rsm_normal;

void main() {
    // Base color
    vec4 base_color_sample = texture(base_color_texture, vertex_texcoord);
    vec4 tinted_base_color = base_color_sample * material.base_color_tint;// * vertex_color;

    vec4 data_sample = texture(data_texture, vertex_texcoord);
    vec4 tinted_data = data_sample * vec4(0.f, material.roughness_factor, material.metalness_factor, 0.f);

    const float dielectric_f0 = 0.04; // TODO: Get this from a texture
    const vec3 f0 = mix(dielectric_f0.xxx, tinted_base_color.rgb, tinted_data.g);

    const vec3 diffuse_color = tinted_base_color.rgb * (1 - dielectric_f0) * (1 - tinted_data.g);

    rsm_flux = vec4(diffuse_color, 1);

    // Normals
    // TODO: Normalmapping
    rsm_normal = vec4(vertex_normal * 0.5f + 0.5f, 0.f);
}
