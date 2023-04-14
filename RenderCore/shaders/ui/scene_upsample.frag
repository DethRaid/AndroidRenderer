#version 460 core

/**
 * Scene upsample - upsamples the scene texture
 *
 * Initial implementation just uses hardware bilinear filtering. Future versions may or may not use
 * something better
 *
 * Should be paired with the fullscreen triangle vertex shader
 */

layout(set = 0, binding = 0) uniform sampler2D scene_color_texture;

layout(set = 0, binding = 1) uniform sampler2D bloom_chain;

layout(location = 0) in vec2 texcoord;

layout(location = 0) out mediump vec4 color_out;

mediump vec3 sample_bloom_chain(vec2 texcoord) {
    mediump vec3 result = vec3(0);

    for(float mip_level = 0; mip_level < 5.0; mip_level += 1) {
        mediump vec3 bloom_sample = textureLod(bloom_chain, texcoord, mip_level).xyz;
        result += bloom_sample;
    }

    return result;
}

mediump float to_luminance(const mediump vec3 color) { return color.r * 0.2126 + color.g * 0.7152 + color.b * 0.0722; }

void main() {
    mediump vec3 bloom = sample_bloom_chain(texcoord);

    mediump vec4 scene_color = textureLod(scene_color_texture, texcoord, 0);

    scene_color.rgb += bloom / 7.0;

    scene_color.rgb = pow(scene_color.rgb, vec3(1.f / 2.2f));

    // Simple reinhard
    mediump float luma = to_luminance(scene_color.rgb);
    mediump float factor = luma / (luma + 1.f);
    mediump vec3 mapped_color = scene_color.rgb * factor;

    color_out = vec4(mapped_color.rgb, 1.f);
}
