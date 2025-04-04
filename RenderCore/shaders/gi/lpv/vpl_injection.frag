#version 460

#extension GL_GOOGLE_include_directive : enable

#include "common/spherical_harmonics.glsl"

layout(location = 0) in mediump vec3 color_in;
layout(location = 1) in mediump vec3 normal_in;

layout(location = 0) out mediump vec4 red;
layout(location = 1) out mediump vec4 green;
layout(location = 2) out mediump vec4 blue;

vec3 rgb2hsv(vec3 c)
{
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    if(length(normal_in) < 1 || length(color_in) == 0) {
        discard;
    }

    mediump vec3 scaled_color = color_in * (32 * 32) / (128 * 128);

    // Boost saturation because yolo
    mediump vec3 hsv_color = rgb2hsv(scaled_color);
    hsv_color.y *= 2;
    mediump vec3 corrected_color = hsv2rgb(hsv_color);

    mediump vec3 normal = normal_in;
    // normal.z *= -1;

    mediump vec4 sh = dir_to_cosine_lobe(normal);

    red = sh * corrected_color.r / PI;
    green = sh * corrected_color.g / PI;
    blue = sh * corrected_color.b / PI;
}
