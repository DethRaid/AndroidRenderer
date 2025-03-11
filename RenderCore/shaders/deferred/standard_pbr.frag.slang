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

[shader("fragment")]
GbufferOutput main(
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
    half3 bitangent = cross(vertex_tangent, vertex_normal);
    half3x3 tbn = half3x3(
        vertex_tangent,
        bitangent,
        vertex_normal
    );
    Sampler2D normals_texture = textures[material.normal_texture_index];
    half3 normal_sample = (half3)normals_texture.SampleBias(vertex_texcoord, camera_data.material_texture_mip_bias).xyz * 2.0 - 1.0;
    half3 normal = mul(tbn, normal_sample);
    gbuffer.normal = half4(vertex_normal, 0);

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
