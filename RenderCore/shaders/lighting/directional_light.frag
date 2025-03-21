#version 460 core

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_ray_query : enable

#include "shared/sun_light_constants.hpp"
#include "shared/view_data.hpp"
#include "common/brdf.glsl"

#define PI 3.1415927

#ifndef medfloat
#define medfloat mediump float
#define medvec2 mediump vec2
#define medvec3 mediump vec3
#define medvec4 mediump vec4
#endif

// Gbuffer textures

layout(set = 0, binding = 0) uniform sampler2D gbuffer_base_color;
layout(set = 0, binding = 1) uniform sampler2D gbuffer_normal;
layout(set = 0, binding = 2) uniform sampler2D gbuffer_data;
layout(set = 0, binding = 3) uniform sampler2D gbuffer_emission;
layout(set = 0, binding = 4) uniform sampler2D gbuffer_depth;

// Sun shadowmaps
layout(set = 1, binding = 0) uniform sampler2DArrayShadow sun_shadowmap;
layout(set = 1, binding = 1) uniform DirectionalLightUbo {
    SunLightConstants sun_light;
};

layout(set = 1, binding = 2) uniform ViewUniformBuffer {
    ViewDataGPU view_info;
};
// layout(set = 1, binding = 3) uniform accelerationStructureEXT rtas;

// Texcoord from the vertex shader
layout(location = 0) in vec2 texcoord;

// Lighting output
layout(location = 0) out medvec4 lighting;

vec3 get_viewspace_position() {
    const float depth = texelFetch(gbuffer_depth, ivec2(gl_FragCoord.xy), 0).r;
    vec2 texcoord = (gl_FragCoord.xy + 0.5) / view_info.render_resolution.xy;
    vec4 ndc_position = vec4(vec3(texcoord * 2.0 - 1.0, depth), 1.f);
    vec4 viewspace_position = view_info.inverse_projection * ndc_position;
    viewspace_position /= viewspace_position.w;

    return viewspace_position.xyz;
}

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

medfloat get_shadow_factor(vec3 worldspace_position, uint cascade_index, float bias) {
    vec4 shadow_lookup = vec4(-1);
    
    vec4 shadowspace_position = biasMat * sun_light.cascade_matrices[cascade_index] * vec4(worldspace_position, 1.0);
    shadowspace_position /= shadowspace_position.w;

    if(any(lessThan(shadowspace_position.xyz, vec3(0))) || any(greaterThan(shadowspace_position.xyz, vec3(1)))) {
        return 1;
    }

    // Use this cascade
    shadow_lookup.xy = shadowspace_position.xy;
    shadow_lookup.z = cascade_index;
    shadow_lookup.w = shadowspace_position.z - bias;
        
    return texture(sun_shadowmap, shadow_lookup);
}

medfloat sample_csm(vec3 worldspace_position, float viewspace_depth, float ndotl) {
    uint cascade_index = 0;
    for(uint i = 0; i < 4; i++) {
        if(viewspace_depth < sun_light.data[i].x) {
            cascade_index = i + 1;
        }
    }

    medfloat shadow = get_shadow_factor(worldspace_position, cascade_index, 0.0005 * sqrt(1 - ndotl * ndotl) / ndotl);
    if(cascade_index > 3) {
        shadow = 0.f;
    }

    return shadow;
}

void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    medvec3 base_color_sample = texelFetch(gbuffer_base_color, pixel, 0).rgb;
    medvec3 normal_sample = normalize(texelFetch(gbuffer_normal, pixel, 0).xyz);
    medvec4 data_sample = texelFetch(gbuffer_data, pixel, 0);
    medvec4 emission_sample = texelFetch(gbuffer_emission, pixel, 0);

    vec3 viewspace_position = get_viewspace_position();
    vec4 worldspace_position = view_info.inverse_view * vec4(viewspace_position, 1.0);

    vec3 view_position = vec3(-view_info.view[3].xyz);
    vec3 worldspace_view_position = worldspace_position.xyz - view_position;
    medvec3 worldspace_view_vector = normalize(worldspace_view_position);

    medvec3 light_vector = normalize(-sun_light.direction_and_tan_size.xyz);

    SurfaceInfo surface;
    surface.base_color = base_color_sample;
    surface.normal = normal_sample;
    surface.roughness = data_sample.g;
    surface.metalness = data_sample.b;
    surface.emission = emission_sample.rgb;
    surface.location = worldspace_position.xyz;

    medfloat ndotl = clamp(dot(normal_sample, light_vector), 0.f, 1.f);

    medfloat shadow = 1;

    if(ndotl > 0) {
        if(sun_light.shadow_mode == SHADOW_MODE_CSM) {
            shadow = sample_csm(worldspace_position.xyz, viewspace_position.z, ndotl);    
        } 
    }

    medvec3 brdf_result = brdf(surface, light_vector, worldspace_view_vector);

    medvec3 direct_light = ndotl * brdf_result * sun_light.color.rgb * shadow;

    // Number chosen based on what happened to look fine
    const medfloat exposure_factor = 0.00031415927f;

    // TODO: https://trello.com/c/4y8bERl1/11-auto-exposure Better exposure
    
    if(any(isnan(direct_light))) {
        direct_light = vec3(0);
    }

    lighting = vec4(direct_light * exposure_factor, 1.f);

    // Shadow cascade visualization
    // TODO: A uniform buffer with debug info?
    // if(cascade_index == 0) {
    //     lighting = vec4(1, 0, 0, 1);
    // } else if(cascade_index == 1) {
    //     lighting = vec4(0, 1, 0, 1);
    // } else if(cascade_index == 2) {
    //     lighting = vec4(0, 0, 1, 1);
    // } else if(cascade_index == 3) {
    //     lighting = vec4(1, 1, 0, 1);
    // } else {
    //     lighting = vec4(1, 0, 1, 1);
    // }

    // lighting = vec4(ndotl.xxx, 1.0);
}
