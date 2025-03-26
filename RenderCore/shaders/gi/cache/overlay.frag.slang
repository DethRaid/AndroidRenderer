// Adds the GI probes to the scene. Has options for both RTGI and LightCache

#include "common/brdf.slangi"
#include "probe_sampling.slangi"
#include "shared/view_data.hpp"

[vk::binding(0, 0)]
Sampler2D<half4> gbuffer_base_color;
[vk::binding(1, 0)]
Sampler2D<half4> gbuffer_normals;
[vk::binding(2, 0)]
Sampler2D<half4> gbuffer_data;
[vk::binding(3, 0)]
Sampler2D<half4> gbuffer_emission;
[vk::binding(4, 0)]
Sampler2D<float> gbuffer_depth;

[vk::binding(0, 1)]
ConstantBuffer<ViewDataGPU> view_data;
[vk::binding(1, 1)]
ConstantBuffer<ProbeCascade[4]> cascades;
[vk::binding(2, 1)]
Sampler3D<half3> probe_irradiance;
[vk::binding(3, 1)]
Sampler3D<half2> probe_depth;
[vk::binding(4, 1)]
Texture3D<half> probe_validity;

[vk::push_constant]
cbuffer Constants {
    uint2 probe_size;
};

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
    int2 pixel = (int2)sv_position.xy;

    const float depth = gbuffer_depth[pixel];
    if (depth == 0.f) {
        discard;
    }

    SurfaceInfo surface = (SurfaceInfo)0;
    surface.base_color = gbuffer_base_color[pixel].rgb;
    surface.normal = normalize(gbuffer_normals[pixel].xyz);

    const half4 data_sample = gbuffer_data[pixel];
    surface.roughness = data_sample.g;
    surface.metalness = data_sample.b;
    surface.location = get_worldspace_location(pixel);
    surface.emission = gbuffer_emission[pixel].rgb;

    const float3 view_position = float3(-view_data.view[3].xyz);
    const float3 worldspace_view_position = surface.location - view_position;
    const half3 worldspace_view_vector = (half3)normalize(worldspace_view_position);

    uint cascade_index = 5;
    for (uint i = 0; i < 4; i++) {
        const float3 cascade_min = cascades[i].min;
        const float3 cascade_max = cascade_min + float3(32, 8, 32) * cascades[i].probe_spacing;
        if (all(surface.location > cascade_min) && all(surface.location < cascade_max)) {
            cascade_index = i;
            break;
        }
    }

    if (cascade_index > 3) {
        return (half4)0;
    }

    const half3 irradiance = sample_cascade(
        surface.location,
        surface.normal,
        cascade_index,
        cascades[cascade_index],
        probe_irradiance,
        probe_size,
        probe_depth,
        probe_validity);

    const half3 brdf_result = brdf(surface, surface.normal, worldspace_view_vector);

    // ndotl is always 1
    return half4(brdf_result * irradiance, 1);
}

