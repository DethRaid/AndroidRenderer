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

layout(location = 0) in vec2 texcoord;

layout(location = 0) out vec4 color_out;

float to_luminance(const vec3 color) { return color.r * 0.2126 + color.g * 0.7152 + color.b * 0.0722; }

void main() {
    vec4 scene_color = textureLod(scene_color_texture, texcoord, 0);

    // Simple reinhard
    float luma = to_luminance(scene_color.rgb);
    float factor = luma / (luma + 1.f);
    vec3 mapped_color = scene_color.rgb * factor;

    color_out = vec4(scene_color.rgb, 1.f);
}
