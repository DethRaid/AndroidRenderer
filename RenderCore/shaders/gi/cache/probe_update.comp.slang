#include "shared/gi_probe.hpp"

StructuredBuffer<uint3> probes_to_update;

StructuredBuffer<ProbeTraceResult> trace_results;

RWTexture3D<half3> rtgi;

RWTexture3D<half3> light_cache;

RWTexture3D<half2> depth;

RWTexture3D<half3> average;

RWTexture3D<half> validity;

// 180 bytes
groupshared half3 local_rtgi[5 * 6];
// 726 bytes
groupshared half3 local_light_cache[11 * 11];
// 200 bytes
groupshared half local_depth[10 * 10];
// 12 bytes
groupshared float3 local_average;
// 4 bytes
groupshared uint num_valid_rays;

// total: 1122 bytes. Vulkan garuntees us at least 16kb

[numthreads(64, 1, 1)]
[shader("compute")]
void main(uint3 group_id: SV_GroupID, uint3 thread_id: SV_GroupThreadID) {
    const uint probe_index = group_id.x;
    const uint3 probe_id = probes_to_update[probe_index];
    const uint cascade_index = probe_id.y / 8;

    // Init memory
    if (thread_id.x < 5 * 6) {
        local_rtgi[thread_id.x] = (half3)0;
    }

    local_light_cache[thread_id.x] = (half3)0;
    if (thread_id.x + 64 < 11 * 11) {
        local_light_cache[thread_id.x + 64] = (half3)0;
    }

    local_depth[thread_id.x] = (half)0;
    if (thread_id.x + 64 < 10 * 10) {
        local_depth[thread_id.x + 64] = (half)0;
    }

    if (thread_id.x == 0) {
        local_average = (float3)0;
        num_valid_rays = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    // Use a rolling window to convolve ray results. The workgroup slides over the ray results for this probe, adding
    // them to groupshared memory. Once we've reached the end, we write out results using another sliding window

    for (uint ray_index = 0; ray_index < 400; ray_index += 64) {
        const ProbeTraceResult result = trace_results[probe_index * 400 + ray_index];

        if (result.ray_direction_and_distance.w < 0) {
            continue;
        }

        InterlockedAdd(local_average.r, result.irradiance.r);
        InterlockedAdd(local_average.g, result.irradiance.g);
        InterlockedAdd(local_average.b, result.irradiance.b);

        InterlockedAdd(num_valid_rays, 1);
    }

    GroupMemoryBarrierWithGroupSync();

    // Write results to textures
    const uint3 base_rtgi_pixel = probe_index * uint3(5, 6, 1);
    const uint3 rtgi_offset = uint3(thread_id.x % 5, thread_id.x / 5, 0);
    if (thread_id.x < 5 * 6) {
        rtgi[base_rtgi_pixel + rtgi_offset] = local_rtgi[thread_id.x];
    }

    const uint3 base_light_cache_pixel = probe_index * uint3(11, 11, 1);
    uint3 light_cache_offset = uint3(thread_id.x % 11, thread_id.x / 11, 0);
    light_cache[base_light_cache_pixel + light_cache_offset] = local_light_cache[thread_id.x];
    if (thread_id.x + 64 < 11 * 11) {
        light_cache_offset = uint3((thread_id.x + 64) % 11, (thread_id.x + 64) / 11, 0);
        light_cache[base_light_cache_pixel + light_cache_offset] = local_light_cache[thread_id.x + 64];
    }

    const uint3 base_depth_pixel = probe_index * uint3(10, 10, 1);
    uint3 depth_offset = uint3(thread_id.x % 10, thread_id.x / 10, 0);
    depth[base_depth_pixel + depth_offset] = local_depth[thread_id.x];
    if (thread_id.x + 64 < 10 * 10) {
        depth_offset = uint3((thread_id.x + 64) % 10, (thread_id.x + 64) / 10, 0);
        depth[base_depth_pixel + depth_offset] = local_depth[thread_id.x + 64];
    }

    if (thread_id.x == 0) {
        average[probe_id] = (half3)(local_average / (float)num_valid_rays);
        validity[probe_id] = num_valid_rays > 0 ? 1.h : 0.h;
    }
}
