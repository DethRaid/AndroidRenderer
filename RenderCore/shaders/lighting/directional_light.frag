#version 460 core

#extension GL_KHR_vulkan_glsl : enable
#extension GL_GOOGLE_include_directive : enable

#include "shared/sun_light_constants.hpp"

#define PI 3.1415927

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

// Sun shadowmaps
layout(set = 1, binding = 0) uniform sampler2DArrayShadow sun_shadowmap;
layout(set = 1, binding = 1) uniform DirectionalLightUbo {
    SunLightConstants sun_light;
};

layout(set = 1, binding = 2) uniform ViewUniformBuffer {
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

float D_GGX(float NoH, float roughness) {
    float k = roughness / (1.0 - NoH * NoH + roughness * roughness);
    return k * k * (1.0 / PI);
}

vec3 F_Schlick(float u, vec3 f0, float f90) { return f0 + (f90 - f0) * pow(1.0 - u, 5.0); }

float V_SmithGGXCorrelated(float NoV, float NoL, float a) {
    float a2 = a * a;
    float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
    float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);

    return 0.5 / (GGXV + GGXL);
}

float Fd_Lambert() { return 1.0 / PI; }

vec3 Fd_Burley(float NoV, float NoL, float LoH, float roughness) {
    float f90 = 0.5 + 2.0 * roughness * LoH * LoH;
    vec3 lightScatter = F_Schlick(NoL, vec3(1.0), f90);
    vec3 viewScatter = F_Schlick(NoV, vec3(1.0), f90);

    return lightScatter * viewScatter * (1.0 / PI);
}

float PDF_GGX(const in float roughness, const in vec3 n, const in vec3 l, const in vec3 v) {
    vec3 h = normalize(v + l);

    const float VoH = clamp(dot(v, h), 0, 1);
    const float NoH = clamp(dot(n, h), 0, 1);

    // D_GGX(NoH, roughness) * NoH / (4.0 + VoH);

    return 1 / (4 * VoH);
}

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

float get_shadow_factor(vec3 worldspace_position, uint cascade_index) {
    vec4 shadow_lookup = vec4(-1);
    
    vec4 shadowspace_position = biasMat * sun_light.cascade_matrices[cascade_index] * vec4(worldspace_position, 1.0);
    shadowspace_position /= shadowspace_position.w;

    if(any(lessThan(shadowspace_position.xy, vec2(0))) || any(greaterThan(shadowspace_position.xy, vec2(1)))) {
        return 1;
    }

    if(shadowspace_position.z <= -1 || shadowspace_position.z >= 1) {
        return 1;
    }

    // Use this cascade
    shadow_lookup.xy = shadowspace_position.xy;
    shadow_lookup.z = cascade_index;
    shadow_lookup.w = shadowspace_position.z - 0.005;
        
    return texture(sun_shadowmap, shadow_lookup);
}

vec3 brdf(in SurfaceInfo surface, vec3 l, const vec3 v) {
    // Remapping from https://google.github.io/filament/Filament.html#materialsystem/parameterization/remapping
    const float dielectric_f0 = 0.04; // TODO: Get this from a texture
    const vec3 f0 = mix(dielectric_f0.xxx, surface.base_color.rgb, surface.metalness);

    const vec3 diffuse_color = surface.base_color.rgb * (1 - dielectric_f0) * (1 - surface.metalness);

    const vec3 h = normalize(v + l);

    float NoV = dot(surface.normal, v) + 1e-5;
    float NoL = dot(surface.normal, l);
    const float NoH = clamp(dot(surface.normal, h), 0, 1);
    const float VoH = clamp(dot(v, h), 0, 1);

    if(NoL <= 0) {
        return vec3(0);
    }

    NoV = abs(NoV);
    NoL = clamp(NoL, 0, 1);

    const float D = D_GGX(NoH, surface.roughness);
    const vec3 F = F_Schlick(VoH, f0, 1.f);
    const float V = V_SmithGGXCorrelated(NoV, NoL, surface.roughness);

    // specular BRDF
    const vec3 Fr = (D * V) * F;

    // diffuse BRDF
    const float LoH = clamp(dot(l, h), 0, 1);
    const vec3 Fd = diffuse_color * Fd_Burley(NoV, NoL, LoH, surface.roughness);

    return Fd + Fr;
}

void main() {
    vec3 base_color_sample = subpassLoad(gbuffer_base_color).rgb;
    vec3 normal_sample = normalize(subpassLoad(gbuffer_normal).xyz * 2.f - 1.f);
    vec4 data_sample = subpassLoad(gbuffer_data);
    vec4 emission_sample = subpassLoad(gbuffer_emission);

    vec3 viewspace_position = get_viewspace_position();
    vec4 worldspace_position = view_info.inverse_view * vec4(viewspace_position, 1.0);

    vec3 view_position = vec3(-view_info.view[3].xyz);
    vec3 worldspace_view_vector = normalize(worldspace_position.xyz - view_position);

    vec3 light_vector = normalize(-sun_light.direction_and_size.xyz);

    SurfaceInfo surface;
    surface.base_color = vec4(base_color_sample, 1.0);
    surface.normal = normal_sample;
    surface.roughness = data_sample.g;
    surface.metalness = data_sample.b;
    surface.emission = emission_sample.rgb;
    surface.location = worldspace_position.xyz;

    uint cascade_index = 0;
    for(uint i = 0; i < 4; i++) {
        if(viewspace_position.z < sun_light.data[i].x) {
            cascade_index = i + 1;
        }
    }

    float shadow = get_shadow_factor(worldspace_position.xyz, cascade_index);

    vec3 brdf_result = brdf(surface, light_vector, worldspace_view_vector);

    float ndotl = clamp(dot(normal_sample, light_vector), 0.f, 1.f);

    vec3 direct_light = ndotl * brdf_result * shadow;

    // Number chosen based on what happened to look fine
    const float exposure_factor = 1.f;

    // TODO: https://trello.com/c/4y8bERl1/11-auto-exposure Better exposure

    // lighting = vec4(direct_light * exposure_factor, 1.f);
    lighting = vec4(surface.base_color.rgb, 1.f);
}
