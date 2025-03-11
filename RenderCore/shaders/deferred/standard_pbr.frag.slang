#include "shared/basic_pbr_material.hpp"
#include "shared/primitive_data.hpp"
#include "shared/view_data.hpp"

[vk::binding(0, 0)]
ConstantBuffer<ViewDataGPU> camera_data;

[vk::binding(1, 0)]
StructuredBuffer<PrimitiveDataGPU> primitive_datas;

[vk::binding(2, 0)]
StructuredBuffer<BasicPbrMaterialGpu> materials;

[vk::binding(0, 1)]
Sampler2D textures[];

struct GbufferOutput {
    half4 base_color;
    half4 normal;
    half4 data;
    half4 emission;
};

float3x3 cotangent_frame(float3 N, float3 p, float2 uv) {
    // get edge vectors of the pixel triangle
    float3 dp1 = ddx(p);
    float3 dp2 = ddy(p);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    // solve the linear system
    float3 dp2perp = cross(dp2, N);
    float3 dp1perp = cross(N, dp1);
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    // construct a scale-invariant frame
    float invmax = rsqrt(max(dot(T, T), dot(B, B)));
    return float3x3(T * invmax, B * invmax, N);
}

[shader("fragment")]
GbufferOutput main(
    const float3 viewspace_position,
    const half3 vertex_normal,
    const half3 vertex_tangent,
    const vec2 vertex_texcoord,
    const half4 vertex_color,
    const uint primitive_id) {

    PrimitiveDataGPU primitive = primitive_datas[primitive_id];
    BasicPbrMaterialGpu material = materials[primitive.material_id];

    GbufferOutput gbuffer = (GbufferOutput)0;

    // Base color
    Sampler2D base_color_texture = textures[material.base_color_texture_index];
    half4 base_color_sample = (half4)base_color_texture.SampleBias(vertex_texcoord, camera_data.material_texture_mip_bias);
    half4 tinted_base_color = base_color_sample * vertex_color * (half4)material.base_color_tint;

    gbuffer.base_color = tinted_base_color;

    // Normals
    float3x3 tbn = cotangent_frame(vertex_normal, viewspace_position, vertex_texcoord);
    Sampler2D normals_texture = textures[material.normal_texture_index];
    half3 normal_sample = (half3)normals_texture.SampleBias(vertex_texcoord, camera_data.material_texture_mip_bias).xyz * 2.0 - 1.0;
    half3 normal = mul(normal_sample, tbn);
    gbuffer.normal = half4(normal, 0);

    // Data
    Sampler2D data_texture = textures[material.data_texture_index];
    half4 data_sample = (half4)data_texture.SampleBias(vertex_texcoord, camera_data.material_texture_mip_bias);
    half4 tinted_data = data_sample * half4(0, (half)material.roughness_factor, (half)material.metalness_factor, 0);

    gbuffer.data = tinted_data;

    // Emission
    Sampler2D emission_texture = textures[material.emission_texture_index];
    half4 emission_sample = (half4)emission_texture.SampleBias(vertex_texcoord, camera_data.material_texture_mip_bias);
    half4 tinted_emission = emission_sample * (half4)material.emission_factor;

    gbuffer.emission = tinted_emission;

    return gbuffer;
}
