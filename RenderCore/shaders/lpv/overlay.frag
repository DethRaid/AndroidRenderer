#version 460 core

#extension GL_GOOGLE_include_directive : enable

#include "common/spherical_harmonics.glsl"
#include "shared/lpv.hpp"

struct ViewInfo {
    mat4 view;
    mat4 projection;

    mat4 inverse_view;
    mat4 inverse_projection;

    vec2 render_resolution;
};

// Gbuffer textures

layout(set = 0, binding = 0, input_attachment_index = 0) uniform subpassInput gbuffer_base_color;
layout(set = 0, binding = 1, input_attachment_index = 1) uniform subpassInput gbuffer_normal;
layout(set = 0, binding = 2, input_attachment_index = 2) uniform subpassInput gbuffer_data;
layout(set = 0, binding = 3, input_attachment_index = 3) uniform subpassInput gbuffer_emission;
layout(set = 0, binding = 4, input_attachment_index = 4) uniform subpassInput gbuffer_depth;

// LPV textures
layout(set = 1, binding = 0) uniform sampler3D lpv_red;
layout(set = 1, binding = 1) uniform sampler3D lpv_green;
layout(set = 1, binding = 2) uniform sampler3D lpv_blue;
layout(set = 1, binding = 3) uniform LpvCascadeBuffer {
    LPVCascadeMatrices cascade_matrices[4];
};

layout(set = 1, binding = 4) uniform ViewUniformBuffer {
    ViewInfo view_info;
};

// Texcoord from the vertex shader
layout(location = 0) in vec2 texcoord;

// Lighting output
layout(location = 0) out vec4 lighting;

vec3 get_viewspace_position() {
    float depth = subpassLoad(gbuffer_depth).r;
    vec2 texcoord = gl_FragCoord.xy / view_info.render_resolution;
    vec4 ndc_position = vec4(vec3(texcoord * 2.0 - 1.0, depth), 1.f);
    vec4 viewspace_position = view_info.inverse_projection * ndc_position;
    viewspace_position /= viewspace_position.w;

    return viewspace_position.xyz;
}

struct SurfaceInfo {
    vec3 location;

    vec4 base_color;

    vec3 normal;

    float metalness;

    float roughness;

    vec3 emission;
};

void main() {
    vec3 base_color_sample = subpassLoad(gbuffer_base_color).rgb;
    vec3 normal_sample = normalize(subpassLoad(gbuffer_normal).xyz * 2.f - 1.f);
    vec4 data_sample = subpassLoad(gbuffer_data);
    vec4 emission_sample = subpassLoad(gbuffer_emission);

    vec3 viewspace_position = get_viewspace_position();
    vec4 worldspace_position = view_info.inverse_view * vec4(viewspace_position, 1.0);

    vec3 view_position = vec3(-view_info.view[3].xyz);
    vec3 worldspace_view_vector = normalize(worldspace_position.xyz - view_position);

    SurfaceInfo surface;
    surface.base_color = vec4(base_color_sample, 1.0);
    surface.normal = normal_sample;
    surface.metalness = data_sample.b;
    surface.roughness = data_sample.g;
    surface.emission = emission_sample.rgb;
    surface.location = worldspace_position.xyz;

    // uint cascade_index = 0;
    // vec4 cascade_position = vec4(0);
    // for(uint i = 0; i < 4; i++) {
    //     cascade_position = world_to_cascade[i] * worldspace_position;
    //     if(all(greaterThan(cascade_position.xyz, vec3(0))) && all(lessThan(cascade_position.xyz, vec3(1)))) {
    //         cascade_index = i;
    //         break;
    //     }
    // }

    vec4 cascade_position = cascade_matrices[0].world_to_cascade * worldspace_position;

    vec4 red_coefficients = texture(lpv_red, cascade_position.xyz);
    vec4 green_coefficients = texture(lpv_green, cascade_position.xyz);
    vec4 blue_coefficients = texture(lpv_blue, cascade_position.xyz);

    vec4 normal_coefficients = sh_project_cone(-surface.normal);

    float red_strength = dot(red_coefficients, normal_coefficients);
    float green_strength = dot(green_coefficients, normal_coefficients);
    float blue_strength = dot(blue_coefficients, normal_coefficients);

    vec3 indirect_light = vec3(red_strength, green_strength, blue_strength);

    const float dielectric_f0 = 0.04; // TODO: Get this from a texture
    const vec3 f0 = mix(dielectric_f0.xxx, surface.base_color.rgb, surface.metalness);

    const vec3 diffuse_color = surface.base_color.rgb * (1 - dielectric_f0) * (1 - surface.metalness);

    vec3 reflection_vector = reflect(-worldspace_view_vector, surface.normal);
    vec4 reflection_coefficients = sh_project_cone(reflection_vector);
    
    // Number chosen based on what happened to look fine
    const float exposure_factor = 1.f;

    // TODO: https://trello.com/c/4y8bERl1/11-auto-exposure Better exposure

    lighting = vec4(diffuse_color * indirect_light * exposure_factor, 1.f);
}
