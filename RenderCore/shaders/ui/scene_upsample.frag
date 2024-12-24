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

mediump vec3 blur(const vec2 uv, const float mip_level) {
    const float delta = 1.f;
    
    vec2 source_size = textureSize(bloom_chain, int(mip_level));
    vec2 inv_source_size = vec2(1.0) / source_size;

    mediump vec4 o = inv_source_size.xyxy * vec2(-delta, delta).xxyy;
    mediump vec3 s = 
        textureLod(bloom_chain, uv, mip_level).rgb * 4.0 +
        textureLod(bloom_chain, uv + vec2(o.x, 0), mip_level).rgb * 2.0 +
        textureLod(bloom_chain, uv + vec2(o.y, 0), mip_level).rgb * 2.0 +
        textureLod(bloom_chain, uv + vec2(0, o.z), mip_level).rgb * 2.0 +
        textureLod(bloom_chain, uv + vec2(0, o.w), mip_level).rgb * 2.0 +
        textureLod(bloom_chain, uv + o.xy, mip_level).rgb * 1.0 +
        textureLod(bloom_chain, uv + o.zy, mip_level).rgb * 1.0 +
        textureLod(bloom_chain, uv + o.xw, mip_level).rgb * 1.0 +
        textureLod(bloom_chain, uv + o.zw, mip_level).rgb * 1.0;

    return s / 16.f;
}

mediump vec3 sample_bloom_chain(vec2 texcoord) {
    vec2 texture_size = textureSize(bloom_chain, 0);
    vec2 half_texel = vec2(1.0) / texture_size;
    mediump vec3 result = vec3(0);

    for(float mip_level = 0; mip_level < 6.0; mip_level += 1) {
        mediump vec3 bloom_sample = blur(texcoord, mip_level);
        result += bloom_sample;
    }

    return result;
}

mediump float to_luminance(const mediump vec3 color) { return color.r * 0.2126 + color.g * 0.7152 + color.b * 0.0722; }

void main() {
    mediump vec3 bloom = sample_bloom_chain(texcoord);

    mediump vec4 scene_color = textureLod(scene_color_texture, texcoord, 0) * 3.1415927;

    // scene_color.rgb += bloom * 0.314159;

    //scene_color.rgb = pow(scene_color.rgb, vec3(1.f / 2.2f));

    // Simple reinhard
    mediump float luma = to_luminance(scene_color.rgb);
    mediump float factor = luma / (luma + 1.f);
    mediump vec3 mapped_color = scene_color.rgb * factor;
    
    mapped_color.rgb = pow(mapped_color, vec3(1.f / 2.2f));

    color_out = vec4(mapped_color.rgb, 1.f);
}
