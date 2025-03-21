#version 460

#extension GL_GOOGLE_include_directive : enable

#include "sky/common.glsl"
#include "shared/view_data.hpp"

#ifndef medfloat
#define medfloat mediump float
#define medvec2 mediump vec2
#define medvec3 mediump vec3
#define medvec4 mediump vec4
#endif

// Implementation of the Hillaire 2020 sky from ShaderToy. This shader applies the sky to any pixel with depth = 1

/*
 * Partial implementation of
 *    "A Scalable and Production Ready Sky and Atmosphere Rendering Technique"
 *    by S�bastien Hillaire (2020).
 * Very much referenced and copied S�bastien's provided code: 
 *    https://github.com/sebh/UnrealEngineSkyAtmosphere
 *
 * This basically implements the generation of a sky-view LUT, so it doesn't
 * include aerial perspective. It only works for views inside the atmosphere,
 * because the code assumes that the ray-marching starts at the camera position.
 * For a planetary view you'd want to check that and you might march from, e.g.
 * the edge of the atmosphere to the ground (rather than the camera position
 * to either the ground or edge of the atmosphere).
 *
 * Also want to cite: 
 *    https://www.shadertoy.com/view/tdSXzD
 * Used the jodieReinhardTonemap from there, but that also made
 * me realize that the paper switched the Mie and Rayleigh height densities
 * (which was confirmed after reading S�bastien's code more closely).
 */

layout(set = 1, binding = 0) uniform sampler2D transmittance_lut;
layout(set = 1, binding = 1) uniform sampler2D sky_view_lut;

layout(set = 1, binding = 2) uniform ViewUniformBuffer {
    ViewDataGPU view_info;
};

layout(set = 0, binding = 0) uniform sampler2D gbuffer_base_color;
layout(set = 0, binding = 1) uniform sampler2D gbuffer_normal;
layout(set = 0, binding = 2) uniform sampler2D gbuffer_data;
layout(set = 0, binding = 3) uniform sampler2D gbuffer_emission;
layout(set = 0, binding = 4) uniform sampler2D gbuffer_depth;

layout(push_constant) uniform Constants {
    vec3 sun_direction;
} constants;

layout(location = 0) out vec3 color;

/*
 * Final output basically looks up the value from the skyLUT, and then adds a sun on top,
 * does some tonemapping.
 */
vec3 getValFromSkyLUT(vec3 rayDir, vec3 sunDir)
{
    float height = length(viewPos);
    vec3 up = viewPos / height;
    
    float horizonAngle = safeacos(sqrt(height * height - groundRadiusMM * groundRadiusMM) / height);
    float altitudeAngle = horizonAngle - acos(dot(rayDir, up)); // Between -PI/2 and PI/2
    float azimuthAngle; // Between 0 and 2*PI
    if (abs(altitudeAngle) > (0.5 * PI - .0001))
    {
        // Looking nearly straight up or down.
        azimuthAngle = 0.0;
    }
    else
    {
        vec3 right = cross(sunDir, up);
        vec3 forward = cross(up, right);
        
        vec3 projectedDir = normalize(rayDir - up * (dot(rayDir, up)));
        float sinTheta = dot(projectedDir, right);
        float cosTheta = dot(projectedDir, forward);
        azimuthAngle = atan(sinTheta, cosTheta) + PI;
    }
    
    // Non-linear mapping of altitude angle. See Section 5.3 of the paper.
    float v = 0.5 + 0.5 * sign(altitudeAngle) * sqrt(abs(altitudeAngle) * 2.0 / PI);
    vec2 uv = vec2(azimuthAngle / (2.0 * PI), v);
        
    return texture(sky_view_lut, uv).rgb;
}

float sunWithBloom(vec3 rayDir, vec3 sunDir)
{
    const float sunSolidAngle = 0.53 * PI / 180.0;
    const float minSunCosTheta = cos(sunSolidAngle);

    float cosTheta = dot(rayDir, sunDir);
    if (cosTheta >= minSunCosTheta)
    {
        return 1.f;
    }
    
    float offset = minSunCosTheta - cosTheta;
    float gaussianBloom = exp(-offset * 50000.0) * 0.5;
    float invBloom = 1.0 / (0.02 + offset * 300.0) * 0.01;
    return gaussianBloom + invBloom;
}

void main() {
    const float depth = texelFetch(gbuffer_depth, ivec2(gl_FragCoord.xy), 0).r;
    if(depth != 1.f) {
        discard;
    }

    // Screen position for the ray
    vec2 location_screen = (gl_FragCoord.xy + 0.5) / view_info.render_resolution.xy;
 
    vec4 location_clipspace = vec4(location_screen, 0.f, 1.f);
    vec4 location_viewspace = view_info.inverse_projection * location_clipspace;
    location_viewspace /= location_viewspace.w;
    vec3 view_vector_worldspace = -normalize((view_info.inverse_view * vec4(location_viewspace.xyz, 0)).xyz);
    view_vector_worldspace.y *= -1;

    vec3 sunDir = -constants.sun_direction;
        
    vec3 lum = getValFromSkyLUT(view_vector_worldspace, sunDir);

    // Bloom should be added at the end, but this is subtle and works well.
    vec3 sunLum = vec3(sunWithBloom(view_vector_worldspace, sunDir));
    // Use smoothstep to limit the effect, so it drops off to actual zero.
    sunLum = smoothstep(0.002, 1.0, sunLum);
    if (length(sunLum) > 0.0)
    {
        if (rayIntersectSphere(viewPos, view_vector_worldspace, groundRadiusMM) >= 0.0)
        {
            sunLum *= 0.0;
        }
        else
        {
            // If the sun value is applied to this pixel, we need to calculate the transmittance to obscure it.
            sunLum *= getValFromTLUT(transmittance_lut, viewPos, sunDir);
        }
    }
    lum += sunLum;

    lum *= 20.f;
    
    // Number chosen based on what happened to look fine
    const medfloat exposure_factor = 1.f;

    lum *= exposure_factor;

    color = lum;

    //color = vec3(view_vector_worldspace);
}
