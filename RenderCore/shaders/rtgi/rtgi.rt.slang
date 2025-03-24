#include "common/brdf.slangi"
#include "shared/sun_light_constants.hpp"
#include "shared/view_data.hpp"
#include "shared/primitive_data.hpp"
#include "util/srgb_conversions.slangi"
#include "rtgi/rtgi_payload.slangi"

// Generates rays for the sun shadow. Uses ray tracing pipelines to get pretty shadows
// The primitive data buffer is bound at 0.0, the other bindings in this file work around it. Check out
// gltf_basic_pbr.slang for other resources we use

[vk::binding(2, 0)]
ConstantBuffer<ViewDataGPU> view_data;

[vk::binding(3, 0)]
RaytracingAccelerationStructure rtas;

[vk::binding(4, 0)]
Texture2D<half4> gbuffer_normals;
[vk::binding(5, 0)]
Texture2D<float> gbuffer_depth;

[vk::binding(6, 0)]
Texture2D<float3> noisetex;

[vk::binding(7, 0)]
RWTexture2D<float4> ray_buffer;

[vk::binding(8, 0)]
RWTexture2D<float4> ray_irradiance;

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

[[vk::push_constant]]
cbuffer Constants {
    uint num_bounces;
};

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

    const half3 normal = gbuffer_normals[thread_id].xyz;

    const float3 location = get_worldspace_location(thread_id);
    
    const uint2 noisetex_pixel = uint2(thread_id) % 128;

    float3 ray_direction = normalize(noisetex[noisetex_pixel].xyz * 2.f - 1.f);
    if (dot(normal, ray_direction) < 0) {
        ray_direction *= -1;
    }

    RayDesc ray;
    ray.Origin = location;
    ray.Direction = ray_direction;
    ray.TMin = 0.01f;
    ray.TMax = 100000.f;

    // irradiance that comes in along the ray
    IrradiancePayload payload = (IrradiancePayload)0;
    payload.remaining_bounces = 0;

    TraceRay(
        rtas,
        RAY_FLAG_NONE,
        0xFF,
        RAY_TYPE_GI,
        1,
        0,
        ray,
        payload);

    if (any(isnan(payload.irradiance))) {
        payload.irradiance = 0;
    }

    // Number chosen based on what happened to look fine
    const float exposure_factor = 0.0031415927f;

    ray_buffer[thread_id] = float4(ray_direction, payload.ray_distance);
    ray_irradiance[thread_id] = float4(payload.irradiance * exposure_factor, 0);
}
