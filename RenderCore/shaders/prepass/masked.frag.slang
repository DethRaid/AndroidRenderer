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

[shader("fragment")]
float4 main(
    const float2 vertex_texcoord,
    const half4 vertex_color,
    const uint primitive_id) {
    PrimitiveDataGPU primitive = primitive_datas[primitive_id];
    BasicPbrMaterialGpu material = materials[primitive.material_id];
    Sampler2D base_color_texture = textures[material.base_color_texture_index];

    half4 base_color_sample = (half4)base_color_texture.Sample(vertex_texcoord);
    half4 tinted_base_color = base_color_sample * vertex_color * (half4)material.base_color_tint;

    if (tinted_base_color.a < material.opacity_threshold) {
        discard;
    }

    return tinted_base_color;
}