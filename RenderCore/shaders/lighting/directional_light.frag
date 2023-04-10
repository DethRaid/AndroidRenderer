#version 460 core

#extension GL_GOOGLE_include_directive : enable

#include "shared/sun_light_constants.hpp"
#include "shared/view_info.hpp"
#include "common/brdf.glsl"

#define PI 3.1415927

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

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

float get_shadow_factor(vec3 worldspace_position, uint cascade_index, float bias) {
    vec4 shadow_lookup = vec4(-1);
    
    vec4 shadowspace_position = biasMat * sun_light.cascade_matrices[cascade_index] * vec4(worldspace_position, 1.0);
    shadowspace_position /= shadowspace_position.w;

    if(any(lessThan(shadowspace_position.xyz, vec3(0))) || any(greaterThan(shadowspace_position.xyz, vec3(1)))) {
        return 0;
    }

    // Use this cascade
    shadow_lookup.xy = shadowspace_position.xy;
    shadow_lookup.z = cascade_index;
    shadow_lookup.w = shadowspace_position.z - bias;
        
    return texture(sun_shadowmap, shadow_lookup);
}

void main() {
    vec3 base_color_sample = subpassLoad(gbuffer_base_color).rgb;
    vec3 normal_sample = normalize(subpassLoad(gbuffer_normal).xyz);
    vec4 data_sample = subpassLoad(gbuffer_data);
    vec4 emission_sample = subpassLoad(gbuffer_emission);

    vec3 viewspace_position = get_viewspace_position();
    vec4 worldspace_position = view_info.inverse_view * vec4(viewspace_position, 1.0);

    vec3 view_position = vec3(-view_info.view[3].xyz);
    vec3 worldspace_view_position = worldspace_position.xyz - view_position;
    vec3 worldspace_view_vector = normalize(worldspace_view_position);

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

    float ndotl = clamp(dot(normal_sample, light_vector), 0.f, 1.f);

    float shadow = get_shadow_factor(worldspace_position.xyz, cascade_index, 0.025 * (1.f - ndotl));
    if(cascade_index > 3) {
        shadow = 1.f;
    }

    vec3 brdf_result = brdf(surface, light_vector, worldspace_view_vector);

    vec3 direct_light = ndotl * brdf_result * sun_light.color.rgb * shadow;

    // Number chosen based on what happened to look fine
    const float exposure_factor = 0.00015f;

    // TODO: https://trello.com/c/4y8bERl1/11-auto-exposure Better exposure

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
}
