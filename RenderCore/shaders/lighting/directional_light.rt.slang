#include "common/brdf.slangi"
#include "shared/sun_light_constants.hpp"
#include "shared/view_data.hpp"
#include "shared/primitive_data.hpp"
#include "util/srgb_conversions.slangi"

// Generates rays for the sun shadow. Uses ray tracing pipelines to get pretty shadows
// The primitive data buffer is bound at 0.0, the other bindings in this file work around it. Check out
// gltf_basic_pbr.slang for other resources we use


[vk::binding(1, 0)]
ConstantBuffer<SunLightConstants> sun_light;

[vk::binding(2, 0)]
ConstantBuffer<ViewDataGPU> view_data;

[vk::binding(3, 0)]
RaytracingAccelerationStructure rtas;

[vk::binding(4, 0)]
RWTexture2D<half4> lit_scene;

[vk::binding(5, 0)]
Texture2D<half4> gbuffer_base_color;
[vk::binding(6, 0)]
Texture2D<half4> gbuffer_normals;
[vk::binding(7, 0)]
Texture2D<half4> gbuffer_data;
[vk::binding(8, 0)]
Texture2D<float> gbuffer_depth;

[vk::binding(9, 0)]
Texture2D<float3> noisetex;

#define RAY_TYPE_OCCLUSION 0
#define RAY_TYPE_GI 1

float3 get_worldspace_location(const uint2 pixel) {
    const float depth = gbuffer_depth[pixel];
    const float2 texcoord = ((float2)pixel + 0.5) / view_data.render_resolution.xy;
    const float4 ndc_position = float4(texcoord * 2.f - 1.f, depth, 1.f);
    float4 viewspace_position = mul(view_data.inverse_projection, ndc_position);
    viewspace_position /= viewspace_position.w;
    const float4 worldspace_position = mul(view_data.inverse_view, viewspace_position);

    return worldspace_position.xyz;
}

float2 r0(uint n) {
#define PHI 1.618033988749895
    return float2(
        fract(2.f + n / PHI),
        fract(3.f + n / PHI)
    );
}

[shader("raygen")]
void main_raygen() {
    uint2 thread_id = DispatchRaysIndex().xy;

    if(any(thread_id >= view_data.render_resolution)) {
        return;
    }

    const float depth = gbuffer_depth[thread_id];
    if (depth == 0.f) {
        return;
    }

    SurfaceInfo surface = (SurfaceInfo)0;
    surface.base_color = gbuffer_base_color[thread_id].rgb;
    surface.normal = normalize(gbuffer_normals[thread_id].xyz);

    const half4 data_sample = gbuffer_data[thread_id];
    surface.roughness = data_sample.g;
    surface.metalness = data_sample.b;
    surface.location = get_worldspace_location(thread_id);

    const float3 light_vector = normalize(-sun_light.direction_and_tan_size.xyz);

    const half ndotl = (half)clamp(dot(light_vector, surface.normal), 0.h, 1.h);

    const float3 view_position = float3(-view_data.view[3].xyz);
    const float3 worldspace_view_position = surface.location - view_position;
    const half3 worldspace_view_vector = (half3)normalize(worldspace_view_position);
    const half3 brdf_result = brdf(surface, (half3)light_vector, worldspace_view_vector);

    float3 sun_radiance = ndotl * brdf_result * sun_light.color.rgb;

    if (ndotl > 0) {
        const float noise_size = sun_light.direction_and_tan_size.w;

        float shadow = 0;
        for (uint i = 0; i < sun_light.num_shadow_samples; i++) {
            const float2 noise_offset = round(r0(i) * 128.f);
            const uint2 noisetex_pixel = uint2(thread_id + noise_offset) % 128;

            const float3 noise = normalize(noisetex[noisetex_pixel].xyz * 2.f - 1.f);
            const float3 ray_direction = normalize(light_vector + noise * noise_size);

            RayDesc ray;
            ray.Origin = surface.location;
            ray.Direction = ray_direction;
            ray.TMin = 0.01f;
            ray.TMax = 100000.f;

            // 0 if we hit something, 1 if not
            uint payload = 1;

            TraceRay(
                rtas,
                RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
                0xFF,
                RAY_TYPE_OCCLUSION,
                1,
                0,
                ray,
                payload);

            shadow += payload;
        }

        sun_radiance *= shadow / sun_light.num_shadow_samples;
    }

    // Number chosen based on what happened to look fine
    const float exposure_factor = 0.00031415927f;

    //if (any(isnan(sun_radiance)) || any(isinf(sun_radiance))) {
    //    sun_radiance = 0;
    //}

    half4 color = lit_scene[thread_id];
    //color.rgb = srgb_to_linear(color.rgb);
    color.rgb += sun_radiance * exposure_factor;
    // color.rgb = linear_to_srgb(color.rgb);
    lit_scene[thread_id] = color;
}
