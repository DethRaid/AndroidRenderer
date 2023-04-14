#version 460

#define PI 3.1415927

layout(set = 0, binding = 0, input_attachment_index = 0) uniform subpassInput gbuffer_base_color;
layout(set = 0, binding = 1, input_attachment_index = 1) uniform subpassInput gbuffer_normal;
layout(set = 0, binding = 2, input_attachment_index = 2) uniform subpassInput gbuffer_data;
layout(set = 0, binding = 3, input_attachment_index = 3) uniform subpassInput gbuffer_emission;
layout(set = 0, binding = 4, input_attachment_index = 4) uniform subpassInput gbuffer_depth;

layout(location = 0) in vec2 texcoord;

layout(location = 0) out mediump vec4 lighting;

void main() {
    mediump vec4 emission_sample = subpassLoad(gbuffer_emission);

    // Number chosen based on what happened to look fine
    const mediump float exposure_factor = 1.f;

    lighting = vec4(emission_sample.rgb * exposure_factor, 1.f);
}
