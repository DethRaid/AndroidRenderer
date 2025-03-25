#include "shared/gi_probe.hpp"
#include "common/octahedral.slangi"

StructuredBuffer<uint3> probes_to_update;

Texture2DArray<half4> trace_params_texture;

Texture2DArray<half4> trace_results_texture;

RWTexture3D<half2> depth_dest;

[numthreads(10, 10, 1)]
[shader("compute")]
void main(uint3 group_id: SV_GroupID, uint3 thread_id: SV_GroupThreadID) {
    const uint probe_index = group_id.x;
    const uint3 probe_id = probes_to_update[probe_index];
    const uint cascade_index = probe_id.y / 8;

    // This thread updates one pixel in the depth texture. We find a cone that represents the depth texel, then sample
    // from the trace results to get the average depth and depth squared

    const float2 octant_coords = get_normalized_octahedral_coordinates(thread_id.xy, 10);
    const half3 direction = (half3)get_octahedral_direction(octant_coords);

    // The source texture is 20x20, we're 10x10. We can sample a 2x2 region of pixels
    const uint2 source_base_pixel = thread_id.xy * 2;

    half depth = 0;
    half num_valid_samples = 0;
    for (uint i = 0; i < 4; i++) {
        const uint2 offset = uint2(i, i >> 1) % uint2(1);
        const uint2 read_pixel = source_base_pixel + offset;
        const half ray_depth = trace_results_texture[uint3(read_pixel, probe_index)].w;
        if (ray_depth > 0) {
            const half3 ray_direction = trace_params_texture[uint3(read_pixel, probe_index)].xyz;
            const half weight = dot(direction, ray_direction);
            depth += ray_depth * weight;
            num_valid_samples += 1.h;
        }
    }

    const uint3 write_pixel = probe_id + uint3(thread_id.xy, 0);
    if (num_valid_samples > 0) {
        depth /= num_valid_samples;
        depth_dest[write_pixel] = half2(depth, depth * depth);
    } else {
        depth_dest[write_pixel] = half2(-1.h);
    }
}
