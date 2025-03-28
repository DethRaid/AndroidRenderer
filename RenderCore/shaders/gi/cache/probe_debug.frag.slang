#include "common/octahedral.slangi"
#include "shared/gi_probe.hpp"
#include "shared/view_data.hpp"

ConstantBuffer<ViewDataGPU> view_data;

ConstantBuffer<ProbeCascade[4]> cascades;

Texture3D<half3> rtgi;

Texture3D<half3> light_cache;

Texture3D<half2> depth;

Texture3D<half3> average;

Texture3D<half> validity;

[vk::push_constant]
cbuffer Constants {
    uint cascade_index;
    uint debug_mode;
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
    probe_id.y += cascade_index + 8;

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

    uint2 probe_size = uint2(5, 6);
    switch (debug_mode) {
    case 0:
        probe_size = uint2(5, 6);
        break;

    case 1:
        probe_size = uint2(11, 11);
        break;

    case 2:
        probe_size = uint2(10, 10);
        break;
    }

    const float2 octant_coord = get_octahedral_coordinates(normal);
    const float3 uv = get_probe_uv(probe_id, octant_coord, probe_size);
    const int3 texel = (int3)(uv * float3(32, 32, 32) * float3(probe_size, 1));

    half3 result;

    switch (debug_mode) {
    case 0:
        result = rtgi[texel];
        break;

    case 1:
        result = light_cache[texel];

    case 2:
        result.xy = depth[texel];
        result.z = 0;
        break;
    }

    return half4(result, 1);
}
