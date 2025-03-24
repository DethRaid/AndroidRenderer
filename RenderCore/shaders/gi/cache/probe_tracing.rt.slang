#include "shared/view_data.hpp"
#include "shared/gi_probe.hpp"
#include "rtgi/rtgi_payload.slangi"

[vk::binding(2, 0)]
StructuredBuffer<uint3> probes_to_update;

[vk::binding(3, 0)]
RaytracingAccelerationStructure rtas;

[vk::binding(4, 0)]
Texture3D<half3> rtgi;

[vk::binding(5, 0)]
Texture3D<half3> light_cache;

[vk::binding(6, 0)]
Texture2D<float3> noisetex;

[vk::binding(7, 0)]
Texture3D<half2> depth;

[vk::binding(8, 0)]
Texture3D<half> validity;

[vk::binding(11, 0)]
ConstantBuffer<ProbeCascade[4]> cascades;

[vk::binding(12, 0)]
RWStructuredBuffer<ProbeTraceResult> new_probe_data;

/**
 * Yeet 400 rays into the aether. Convolve them into our textures. Party
 *
 * This shader doesn't update the irradiance cache itself. Rather, we write the new probe data to the new_probe_data
 * buffer. A later compute shader will take that information, generate the RTGI and Average data, and write it all to 
 * irradiance textures. This helps with sync - the RT shaders can sample the irradiance textures as needed, without
 * race conditions between this shader
 */
[shader("raygen")]
void main_raygen() {
    const uint3 thread_id = DispatchRaysIndex();
    const uint ray_index = thread_id.x;
    const uint probe_index = thread_id.y;

    const uint3 probe_id = probes_to_update[probe_index];

    const uint cascade_index = probe_id.y / 8;
    const ProbeCascade cascade = cascades[cascade_index];

    const float3 local_probe_id = (float3)uint3(probe_id.x, probe_id.y % 8, probe_id.z);

    const int2 noise_pixel = int2(ray_index % 128, ray_index / 128);
    const float3 ray_direction = normalize(noisetex[noise_pixel].xyz * 2.f - 1.f);

    float ray_distance = cascade.probe_spacing * 4;
    if (cascade_index < 3) {
        ray_distance = cascades[cascade_index + 1].probe_spacing * 2;
    }

    RayDesc ray = (RayDesc)0;
    ray.Origin = cascade.min + local_probe_id * cascade.probe_spacing;
    ray.Direction = ray_direction;
    ray.TMin = 0;
    ray.TMax = ray_distance;

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

    // TODO: If the ray misses (trace distance = 0), sample the next-highest cascade (or the sky, if we're already in cascade 3)

    if (payload.ray_distance == 0) {
        //payload.irradiance = sample_cascade_or_sky(ray.Origin + ray.Direction * ray.TMax, cascade_index + 1);
        payload.ray_distance = ray_distance;
    }

    const uint write_index = probe_index * 400 + ray_index;
    
    new_probe_data[write_index].irradiance = half4((half3)payload.irradiance, 0);
    new_probe_data[write_index].ray_direction_and_distance = half4((half3)ray_direction, (half)payload.ray_distance);
}
