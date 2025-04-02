StructuredBuffer<uint3> probes_to_update;

RWTexture2DArray<half3> rtgi_dest;

RWTexture2DArray<half2> depth_dest;

RWTexture2DArray<half3> average_dest;

RWTexture2DArray<half> validity_dest;

groupshared uint num_valid_samples;

[numthreads(64, 1, 1)]
[shader("compute")]
void main(uint3 thread_id: SV_DispatchThreadID) {
    const uint probe_index = thread_id.z;
    const uint3 probe_id = probes_to_update[probe_index];

    // We need to do a few things

    // First, compute the validity. If any of the returned depth values are negative, the probe is not valid

    if (thread_id.x == 0) {
        num_valid_samples = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    const uint3 base_depth_texel = probe_id * uint3(12, 12, 1);

    for (uint idx = 0; idx < 100; idx += 64) {
        const uint x = idx % 10;
        const uint y = idx / 10;

        if (y >= 10) {
            continue;
        }

        // Add 1 to account for the 1-pixel border around each probe's data
        const uint2 depth_texel = uint2(x, y) + 1;
        const uint3 texel = base_depth_texel + uint3(depth_texel, 0);

        const half2 depth = depth_dest[texel];
        if (depth.x > 0) {
            InterlockedAdd(num_valid_samples, 1);
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (thread_id.x == 0) {
        validity_dest[probe_id] = (half)num_valid_samples / 100.h;
    }

    const uint3 base_rtgi_texel = probe_id * uint3(7, 8, 1);

    // Next, compute the average
    if (thread_id.x < 30) {
        const uint x = thread_id.x % 5;
        const uint y = thread_id.x / 6;

        // Add 1 to account for the 1-pixel border around each probe's data
        const uint2 rtgi_texel = uint2(x, y) + 1;
        const uint3 texel = base_rtgi_texel + uint3(rtgi_texel, 0);

        const half3 irradiance = rtgi_dest[texel];

        const half3 irradiance_average = WaveActiveSum(irradiance) / 30.h;

        if (thread_id.x == 0) {
            average_dest[probe_id] = irradiance_average;
        }
    }
}
