#include "common/brdf.slangi"
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
Texture2D<half2> noise;
[vk::binding(2, 1)]
Texture2D<half4> ray_buffer;
[vk::binding(3, 1)]
Texture2D<half4> ray_irradiance;

[vk::push_constant]
cbuffer Constants {
    uint num_extra_rays;
    float extra_ray_radius;
};

float2 r1(uint n) {
#define PHI 1.618033988749895
    return float2(
        fract(2.f + n / PHI),
        fract(3.f + n / PHI)
    );
}

float3 get_worldspace_location(const uint2 pixel) {
    const float depth = gbuffer_depth[pixel];
    const float2 texcoord = ((float2)pixel + 0.5) / view_data.render_resolution.xy;
    const float4 ndc_position = float4(texcoord * 2.f - 1.f, depth, 1.f);
    float4 viewspace_position = mul(view_data.inverse_projection, ndc_position);
    viewspace_position /= viewspace_position.w;
    const float4 worldspace_position = mul(view_data.inverse_view, viewspace_position);

    return worldspace_position.xyz;
}

struct IrradiancePath {
    half3 direction;
    half3 irradiance;
};

IrradiancePath get_path(in uint2 pixel) {
    IrradiancePath path;
    path.direction = ray_buffer[pixel].xyz;
    path.irradiance = ray_irradiance[pixel].rgb;

    return path;
}

half3 calc_contribution_from_ray(in SurfaceInfo surface, in half3 view_vector, in IrradiancePath path) {
    const half3 brdf_result = brdf(surface, path.direction, view_vector);
    const half ndotl = (half)clamp(dot(path.direction, surface.normal), 0, 1);
    return brdf_result * path.irradiance * ndotl;
}

[shader("fragment")]
float4 main(float4 sv_position: SV_Position) {
    int2 pixel = (int2)sv_position.xy;

    SurfaceInfo surface = (SurfaceInfo)0;
    surface.base_color = gbuffer_base_color[pixel].rgb;
    surface.normal = normalize(gbuffer_normals[pixel].xyz);

    const half4 data_sample = gbuffer_data[pixel];
    surface.roughness = data_sample.g;
    surface.metalness = data_sample.b;
    surface.location = get_worldspace_location(pixel);
    surface.emission = gbuffer_emission[pixel].rgb;

    const float3 view_position = vec3(-view_data.view[3].xyz);
    const float3 worldspace_view_position = surface.location - view_position;
    const half3 worldspace_view_vector = (half3)normalize(worldspace_view_position);

    const IrradiancePath main_path = get_path(pixel);

    half3 radiance = calc_contribution_from_ray(surface, worldspace_view_vector, main_path);
    uint num_samples = 1;

    for (uint extra_ray = 0; extra_ray < num_extra_rays; extra_ray++) {
        const uint2 noise_offset = (uint2)(r1(extra_ray) * 128);
        const half2 noise_sample = noise[(pixel + noise_offset) % 128] * 2.h - 1.h;

        const uint2 offset_pixel = (uint2)round(pixel + noise_sample * extra_ray_radius);

        const float3 other_location = get_worldspace_location(offset_pixel);
        if (length(surface.location - other_location) > 2) {
            continue;
        }

        const IrradiancePath path = get_path(offset_pixel);

        radiance += calc_contribution_from_ray(surface, worldspace_view_vector, path);
        num_samples++;
    }

    if (any(isnan(radiance))) {
        radiance = 0.h;
    }

    return float4(radiance / (half)num_samples, 1);

    //for (int y_offset = -1; y_offset < 2; y_offset++) {
    //    for (int x_offset = -1; x_offset < 2; x_offset++) {
    //        const IrradiancePath path = get_path(pixel + int2(x_offset, y_offset));
    //        radiance += calc_contribution_from_ray(surface, worldspace_view_vector, path);
    //    }
    //}
//
    //return half4(radiance / 9.h, 1);
}
