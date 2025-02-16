#version 460

#define PI 3.1415927

layout(set = 0, binding = 0) uniform sampler2D gbuffer_base_color;
layout(set = 0, binding = 1) uniform sampler2D gbuffer_normal;
layout(set = 0, binding = 2) uniform sampler2D gbuffer_data;
layout(set = 0, binding = 3) uniform sampler2D gbuffer_emission;
layout(set = 0, binding = 4) uniform sampler2D gbuffer_depth;

layout(location = 0) in vec2 texcoord;

layout(location = 0) out mediump vec4 lighting;

void main() {
    mediump vec4 emission_sample = texelFetch(gbuffer_emission, ivec2(gl_FragCoord.xy), 0);

    // Number chosen based on what happened to look fine
    const mediump float exposure_factor = 3.1415927;

    lighting = vec4(emission_sample.rgb * exposure_factor, 1.f);
}
