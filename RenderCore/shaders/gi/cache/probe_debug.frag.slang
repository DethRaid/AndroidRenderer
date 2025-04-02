#include "common/octahedral.slangi"
#include "shared/gi_probe.hpp"
#include "shared/view_data.hpp"

ConstantBuffer<ViewDataGPU> view_data;

ConstantBuffer<IrradianceProbeVolume> probe_volume;

Sampler2DArray<half3> rtgi;

Sampler2DArray<half3> light_cache;

//Sampler2DArray<half2> depth;

Texture2DArray<half3> average;

Texture2DArray<half> validity;

[vk::push_constant]
cbuffer Constants {
    uint debug_mode;
    uint cascade_index;
};

struct VertexOutput {
    half2 offset;
    u16vec3 probe_id;
};

[shader("fragment")]
half4 main(VertexOutput vertex) {
    const half distance_from_center = length(vertex.offset);
    if (distance_from_center > 1.f) {
        discard;
    }

    uint3 probe_id = vertex.probe_id;
    probe_id.y += cascade_index * 8;

    if (debug_mode == 3) {
        // Sample average and return
        const half3 average_irradiance = average[probe_id];
        return half4(average_irradiance, 1);
    }

    if (debug_mode == 4) {
        // Sample validity and return
        const half probe_validity = validity[probe_id];
        return half4(probe_validity.xxx, 1);
    }

    half3 normal;
    normal.xy = vertex.offset;
    normal.z = sqrt(1.h - vertex.offset.x * vertex.offset.x - vertex.offset.y * vertex.offset.y);
    normal = normalize(normal);
    normal = (half3)mul(view_data.inverse_view, float4(normal, 0)).xyz;

    uint2 probe_size = uint2(5, 6);
    switch (debug_mode) {
    case 0:
        probe_size = probe_volume.rgti_probe_resolution;
        break;

    case 1:
        probe_size = probe_volume.light_cache_probe_resolution;
        break;

    case 2:
        probe_size = probe_volume.depth_probe_resolution;
        break;
    }

    const float2 octant_coord = get_octahedral_coordinates(normal);
    const float3 uv = get_probe_uv(probe_id, octant_coord, probe_size);

    half3 result;

    switch (debug_mode) {
    case 0:
        result = rtgi.SampleLevel(uv, 0);
        break;

    case 1:
        result = light_cache.SampleLevel(uv, 0);

    case 2:
        //result.xy = depth.SampleLevel(uv, 0);
        result.z = 0;
        break;
    }

    return half4(result * 0.01h, 1);

    // result = (half3)probe_id / half3(32, 8, 32);
    //result = half3(octant_coord * 0.5 + 0.5, 0);

    // return half4(result, 1);
}
