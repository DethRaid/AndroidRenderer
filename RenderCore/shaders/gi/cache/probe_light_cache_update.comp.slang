#include "common/octahedral.slangi"
#include "probe_update.slangi"
#include "shared/gi_probe.hpp"

StructuredBuffer<uint3> probes_to_update;

Texture2DArray<half4> trace_params_texture;

Texture2DArray<half4> trace_results_texture;

RWTexture3D<half3> light_cache_dest;

[numthreads(11, 11, 1)]
[shader("compute")]
void main(uint3 group_id: SV_GroupID, uint3 thread_id: SV_GroupThreadID) {
    const uint probe_index = group_id.x;
    const uint3 probe_id = probes_to_update[probe_index];
    const uint cascade_index = probe_id.y / 8;

    // This thread updates one pixel in the depth texture. We find a cone that represents the depth texel, then sample
    // from the trace results to get the average depth and depth squared

    const float2 octant_coords = get_normalized_octahedral_coordinates(thread_id.xy, 11);
    const half3 direction = (half3)get_octahedral_direction(octant_coords);

    // The source texture is 20x20, we're 11x11. We can sample a 2x2 region of pixels
    const uint2 source_base_pixel = (uint2)floor((float2)thread_id.xy * 20.f / 11.f);

    half3 light = (half3)0;
    half num_valid_samples = 0;
    for (uint i = 0; i < 4; i++) {
        const uint2 offset = uint2(i, i >> 1) % uint2(1);
        const uint2 read_pixel = source_base_pixel + offset;
        const half4 irradiance = trace_results_texture[uint3(read_pixel, probe_index)];
        if (irradiance.w > 0) {
            const half3 ray_direction = trace_params_texture[uint3(read_pixel, probe_index)].xyz;
            const half weight = dot(direction, ray_direction);
            light += irradiance.rgb * weight;
            num_valid_samples += 1.h;
        }
    }

    const uint3 write_pixel = probe_id + uint3(thread_id.xy, 0);
    if (num_valid_samples > 0) {
        light /= num_valid_samples;
    } else {
        light = (half3)0;
    }

    write_probe_texel_with_border<3>(light_cache_dest, int2(11, 11), probe_id, thread_id.xy, light);
}
