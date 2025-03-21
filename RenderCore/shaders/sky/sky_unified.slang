// Unifie3d sky shader - contains both the fragment shader to draw the sky in the lighting pass, and the miss shaders
// for the sky for both occlusion and radiance rays

#ifndef SAH_RT
#define SAH_RT 0
#endif

#ifndef SAH_RT_OCCLUSION
#define SAH_RT_OCCLUSION 0
#endif

#ifndef SAH_RT_GI
#define SAH_RT_GI 0
#endif

// Implementation of the Hillaire 2020 sky from ShaderToy. This shader applies the sky to any pixel with depth = 1

#ifndef PI
#define PI 3.14159265358
#endif

#include "shared/sun_light_constants.hpp"
#include "shared/view_data.hpp"


// Units are in megameters.
static const float groundRadiusMM = 6.360;
static const float atmosphereRadiusMM = 6.460;

// 200M above the ground.
static const float3 viewPos = float3(0.0, groundRadiusMM + 0.0002, 0.0);

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

float safeacos(const float x) {
    return acos(clamp(x, -1.0, 1.0));
}

// From https://gamedev.stackexchange.com/questions/96459/fast-ray-sphere-collision-code.
float rayIntersectSphere(float3 ro, float3 rd, float rad) {
    float b = dot(ro, rd);
    float c = dot(ro, ro) - rad * rad;
    if (c > 0.0f && b > 0.0) {
        return -1.0;
    }
    float discr = b * b - c;
    if (discr < 0.0) {
        return -1.0;
    }
    // Special case: inside sphere, use far discriminant
    if (discr > b * b) {
        return (-b + sqrt(discr));
    }
    return -b - sqrt(discr);
}

/*
 * Final output basically looks up the value from the skyLUT, and then adds a sun on top,
 * does some tonemapping.
 */
float3 getValFromSkyLUT(float3 rayDir, float3 sunDir, Sampler2D sky_view_lut)
{
    float height = length(viewPos);
    float3 up = viewPos / height;

    float horizonAngle = safeacos(sqrt(height * height - groundRadiusMM * groundRadiusMM) / height);
    float altitudeAngle = horizonAngle - acos(dot(rayDir, up)); // Between -PI/2 and PI/2
    float azimuthAngle;                                         // Between 0 and 2*PI
    if (abs(altitudeAngle) > (0.5 * PI - .0001))
    {
        // Looking nearly straight up or down.
        azimuthAngle = 0.0;
    }
    else
    {
        float3 right = cross(sunDir, up);
        float3 forward = cross(up, right);

        float3 projectedDir = normalize(rayDir - up * (dot(rayDir, up)));
        float sinTheta = dot(projectedDir, right);
        float cosTheta = dot(projectedDir, forward);
        azimuthAngle = atan(cosTheta / sinTheta) + PI;
    }

    // Non-linear mapping of altitude angle. See Section 5.3 of the paper.
    float v = 0.5 + 0.5 * sign(altitudeAngle) * sqrt(abs(altitudeAngle) * 2.0 / PI);
    float2 uv = float2(azimuthAngle / (2.0 * PI), v);

    return sky_view_lut.SampleLevel(uv, 0).rgb;
}

float3 getValFromTLUT(Sampler2D tex, float3 pos, float3 sunDir) {
    float height = length(pos);
    float3 up = pos / height;
    float sunCosZenithAngle = dot(sunDir, up);
    float2 uv = float2(clamp(0.5 + 0.5 * sunCosZenithAngle, 0.0, 1.0),
                       max(0.0, min(1.0, (height - groundRadiusMM) / (atmosphereRadiusMM - groundRadiusMM))));
    return tex.SampleLevel(uv, 0).rgb;
}

float sunWithBloom(float3 rayDir, float3 sunDir)
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

float3 get_sky_color(in float3 view_vector_worldspace, in float3 sunDir, Sampler2D sky_view_lut, Sampler2D transmittance_lut) {
    float3 lum = getValFromSkyLUT(view_vector_worldspace, sunDir, sky_view_lut);

    // Bloom should be added at the end, but this is subtle and works well.
    float3 sunLum = float3(sunWithBloom(view_vector_worldspace, sunDir));
    // Use smoothstep to limit the effect, so it drops off to actual zero.
    sunLum = smoothstep(0.002h, 1.0h, sunLum);
    if (length(sunLum) > 0.0)
    {
        if (rayIntersectSphere(viewPos, view_vector_worldspace, groundRadiusMM) >= 0.0)
        {
            sunLum = 0;
        }
        else
        {
            // If the sun value is applied to this pixel, we need to calculate the transmittance to obscure it.
            sunLum *= getValFromTLUT(transmittance_lut, viewPos, sunDir);
        }
    }
    lum += sunLum;

    lum *= 20.h;

    // Number chosen based on what happened to look fine
    const float exposure_factor = 1.h;

    lum *= exposure_factor;

    return lum;
}

#if !SAH_RT

[vk::binding(0, 0)]
Sampler2D transmittance_lut;

[vk::binding(1, 0)]
Sampler2D sky_view_lut;

[vk::binding(2, 0)]
ConstantBuffer<ViewDataGPU> view_info;

[vk::binding(3, 0)]
ConstantBuffer<SunLightConstants> sun_info;

[shader("fragment")]
half4 main_fs(float4 fragcoord: SV_Position) {
    // Screen position for the ray
    float2 location_screen = fragcoord.xy / view_info.render_resolution.xy;

    float4 location_clipspace = float4(location_screen, 0.f, 1.f);
    float4 location_viewspace = mul(view_info.inverse_projection, location_clipspace);
    location_viewspace /= location_viewspace.w;
    float3 view_vector_worldspace = -normalize(mul(view_info.inverse_view, float4(location_viewspace.xyz, 0)).xyz);
    view_vector_worldspace.y *= -1;

    float3 sunDir = -normalize(sun_info.direction_and_tan_size.xyz);

    const float3 sky_color = get_sky_color(view_vector_worldspace, sunDir, sky_view_lut, transmittance_lut);

    return half4((half3)sky_color, 1);
}
#else

#if SAH_RT_OCCLUSION
[shader("miss")]
void main_miss(inout uint payload) {
    payload = 1;
}
#endif

#if SAH_RT_GI

#include "rtgi/rtgi_payload.slangi"

[vk::binding(1, 0)]
ConstantBuffer<SunLightConstants> sun_light;

[vk::binding(9, 0)]
Sampler2D transmittance_lut;

[vk::binding(10, 0)]
Sampler2D sky_view_lut;

[shader("miss")]
void main_miss(inout IrradiancePayload payload) {
    payload.irradiance = get_sky_color(WorldRayDirection(), sun_light.direction_and_tan_size.xyz, sky_view_lut, transmittance_lut);
}
#endif

#endif  // SAH_RT
