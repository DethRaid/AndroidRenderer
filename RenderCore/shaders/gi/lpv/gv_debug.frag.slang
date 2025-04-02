#include "shared/view_data.hpp"
#include "shared/lpv.hpp"
#include "common/spherical_harmonics.slangi"

ConstantBuffer<ViewDataGPU> view_data;

ConstantBuffer<LPVCascadeMatrices[4]> cascade_data;

Sampler3D<half4> geo_volume;

Texture2D<float> gbuffer_depth;

float3 get_worldspace_location(const uint2 pixel) {
    const float depth = gbuffer_depth[pixel];
    const float2 texcoord = ((float2)pixel + 0.5) / view_data.render_resolution.xy;
    const float4 ndc_position = float4(texcoord * 2.f - 1.f, depth, 1.f);
    float4 viewspace_position = mul(view_data.inverse_projection, ndc_position);
    viewspace_position /= viewspace_position.w;
    const float4 worldspace_position = mul(view_data.inverse_view, viewspace_position);

    return worldspace_position.xyz;
}

[shader("fragment")]
half4 main(float4 sv_position: SV_Position) {
    uint2 pixel = (uint2)sv_position.xy;

    // Raymarch from camera location to depth buffer location. If we encounter a GV cell with non-zero data, apply simple shading and return

    const float3 worldspace_location = get_worldspace_location(pixel);

    const float3 start_location = -view_data.view[3].xyz;
    const float3 worldspace_view_vector = normalize(worldspace_location.xyz - start_location);

    const float ray_length = 32.f;

    for (float t = 0; t < ray_length; t += 0.1) {
        const float3 cur_location = start_location + worldspace_view_vector * t;
        const float4 cascadespace_location = mul(cascade_data[0].world_to_cascade, float4(cur_location, 1));
        float3 gv_coordinate = cascadespace_location.xyz;
        if (any(gv_coordinate < 0) || any(gv_coordinate > 1)) {
            return half4(0.5, 0.5, 0.5, 1);
        }

        // Compress to the first cascade
        gv_coordinate.x /= 4.f;

        // Add a half-cell offset to the GV
        // + 0.5f / float3(128.f, 32.f, 32.f);
        const half4 gv_sh = geo_volume.SampleLevel(gv_coordinate, 0);
        if (dot(gv_sh, gv_sh) > 0.h) {
            const half4 sun_lobe = dir_to_sh(normalize(half3(-0.1h, 1, 1)));
            const half lighting = dot(gv_sh, sun_lobe);
            return half4(lighting.xxx, 1);
        }
    }

    return half4(0, 0, 1, 1);
}
