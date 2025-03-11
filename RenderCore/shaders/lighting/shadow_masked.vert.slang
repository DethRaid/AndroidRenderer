#include "shared/primitive_data.hpp"
#include "shared/sun_light_constants.hpp"
#include "shared/vertex_data.hpp"

[[vk::binding(0, 0)]]
ConstantBuffer<SunLightConstants> sun_light;

[[vk::binding(1, 0)]]
StructuredBuffer<PrimitiveDataGPU> primitive_datas;

struct VertexOutput {
    float4 position : SV_Position;
    float2 texcoord;
    half4 color;
    uint primitive_id;
};

[shader("vertex")]
VertexOutput main(
    const float3 position_in,
    const float3 normal_in,
    const float3 tangent_in,
    const float2 texcoord_in,
    const half4 color_in,
    const uint primitive_id_in,
    const uint view_index : SV_ViewID
) {
    VertexOutput output;

    output.primitive_id = primitive_id_in;
    PrimitiveDataGPU data = primitive_datas[output.primitive_id];

    output.position = mul(sun_light.cascade_matrices[view_index], mul(data.model, float4(position_in, 1.f)));

    output.texcoord = texcoord_in;
    output.color = color_in;

    return output;
}
