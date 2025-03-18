#include "common/brdf.slangi"
#include "shared/sun_light_constants.hpp"
#include "shared/view_data.hpp"
#include "shared/primitive_data.hpp"

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

#define RAY_TYPE_OCCLUSION 0
#define RAY_TYPE_GI 1

float3 get_worldspace_location(const uint2 pixel) {
    const float depth = gbuffer_depth[pixel];
    const float2 texcoord = (float2)pixel / (float2)view_data.render_resolution;
    const float4 ndc_position = float4(texcoord * 2 - 1, depth, 1);
    float4 viewspace_position = mul(view_data.inverse_projection, ndc_position);
    viewspace_position /= viewspace_position.w;
    const float4 worldspace_position = mul(view_data.inverse_view, viewspace_position);

    return worldspace_position.xyz;
}

[shader("raygen")]
void main_raygen() {
    uint2 thread_id = DispatchRaysIndex().xy;

    if (any(thread_id >= view_data.render_resolution)) {
        return;
    }

    SurfaceInfo surface = (SurfaceInfo)0;
    surface.base_color = gbuffer_base_color[thread_id].rgb;
    surface.normal = gbuffer_normals[thread_id].xyz;

    const half4 data_sample = gbuffer_data[thread_id];
    surface.roughness = data_sample.g;
    surface.metalness = data_sample.b;
    surface.location = (half3)get_worldspace_location(thread_id);

    const float3 light_vector = normalize(-sun_light.direction_and_size.xyz);

    const float ndotl = dot(light_vector, surface.normal);

    const float3 view_position = vec3(-view_data.view[3].xyz);
    const float3 worldspace_view_position = surface.location - view_position;
    const half3 worldspace_view_vector = (half3)normalize(worldspace_view_position);
    const half3 brdf_result = brdf(surface, (half3)light_vector, worldspace_view_vector);

    half3 sun_radiance = brdf_result * (half3)sun_light.color.rgb * (half)ndotl;

    if (ndotl > 0) {
        // Setup the ray
        RayDesc ray;
        ray.Origin = surface.location;
        ray.Direction = light_vector;
        ray.TMin = 0.1f;
        ray.TMax = 100000.f;

        // Trace the ray

        // 0 if we hit something, 1 if not
        uint payload = 1;

        TraceRay(
            rtas,
            RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
            0xFF,
            RAY_TYPE_OCCLUSION,
            0,
            0,
            ray,
            payload);

        // TODO: Send multiple rays, average them
        const half shadow = (half)payload / 1;
        sun_radiance *= shadow;
    }

    lit_scene[thread_id] = half4(sun_radiance, 1);
}

[shader("miss")]
void main_miss(inout uint payload) {
    payload = 1;
}
