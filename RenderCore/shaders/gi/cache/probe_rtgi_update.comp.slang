#include "shared/gi_probe.hpp"
#include "common/octahedral.slangi"
#include "probe_update.slangi"

StructuredBuffer<uint3> probes_to_update;

Texture2DArray<half4> trace_params_texture;

Texture2DArray<half4> trace_results_texture;

RWTexture3D<half3> rtgi_dest;

[numthreads(5, 6, 1)]
[shader("compute")]
void main(uint3 group_id: SV_GroupID, uint3 thread_id: SV_GroupThreadID) {
    const uint probe_index = group_id.x;
    const uint3 probe_id = probes_to_update[probe_index];
    const uint cascade_index = probe_id.y / 8;

    // This thread updates one pixel in the depth texture. We find a cone that represents the depth texel, then sample
    // from the trace results to get the average depth and depth squared

    const float2 octant_coords = get_normalized_octahedral_coordinates(thread_id.xy, uint2(5, 6));
    const half3 direction = (half3)get_octahedral_direction(octant_coords);

    // The source texture is 20x20, we're 5x6. We can sample a 4x4 region of pixels
    const uint2 source_base_pixel = (uint2)floor((float2)thread_id.xy * 20.f / 5.f);

    half3 light = (half3)0;
    half num_valid_samples = 0;
    for (uint i = 0; i < 16; i++) {
        const uint2 offset = uint2(i % 4, i >> 2);
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
        light = 0.h;
    }

    write_probe_texel_with_border<3>(rtgi_dest, int2(5, 6), probe_id, thread_id.xy, light);
}
