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
Texture2D<half4> ray_buffer;
[vk::binding(2, 1)]
Texture2D<half4> ray_irradiance;

[vk::push_constant]
cbuffer Constants {
    uint num_extra_rays;
    float extra_ray_radius;
};

float2 r0(uint n) {
#define PHI 1.618033988749895
    return float2(
        fract(2.f + n / PHI),
        fract(3.f + n / PHI)
    );
}

float3 get_worldspace_location(const uint2 pixel) {
    const float depth = gbuffer_depth[pixel];
    const float2 texcoord = (float2)pixel / view_data.render_resolution.xy;
    const float4 ndc_position = float4(texcoord * 2.f - 1.f, depth, 1.f);
    float4 viewspace_position = mul(view_data.inverse_projection, ndc_position);
    viewspace_position /= viewspace_position.w;
    const float4 worldspace_position = mul(view_data.inverse_view, viewspace_position);

    return worldspace_position.xyz;
}

half3 calc_contribution_from_ray(in SurfaceInfo surface, in half3 view_vector, in half3 ray_direction, in half3 ray_irradiance) {
    const half3 brdf_result = brdf(surface, ray_direction, view_vector);
    const half ndotl = (half)clamp(dot(ray_direction, surface.normal), 0, 1);
    return brdf_result * ray_irradiance * ndotl;
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

    const half3 main_ray_direction = ray_buffer[pixel].xyz;
    const half3 main_ray_irradiance = ray_irradiance[pixel].rgb;

    half3 radiance = calc_contribution_from_ray(surface, worldspace_view_vector, main_ray_direction, main_ray_irradiance);

   // for ()

    return float4(radiance, 1);
}
