#include "shared/primitive_data.hpp"
#include "shared/view_data.hpp"
#include "shared/vertex_data.hpp"

[[vk::binding(0, 0)]]
ConstantBuffer<ViewDataGPU> camera_data;

[[vk::binding(1, 0)]]
StructuredBuffer<PrimitiveDataGPU> primitive_datas;

struct VertexOutput {
    float4 position : SV_Position;
    float3 viewspace_position;
    half3 normal;
    half3 tangent;
    float2 texcoord;
    half4 color;
    uint primitive_id;
};

half4 unpack_color(const uint color) {
    uint r = color & 0xFF;
    uint g = (color >> 8) & 0xFF;
    uint b = (color >> 16) & 0xFF;
    uint a = (color >> 24) & 0xFF;

    return half4((half)r / 255, (half)g / 255, (half)b / 255, (half)a / 255);
}

[shader("vertex")]
VertexOutput main(
    const float3 position_in,
    const float3 normal_in,
    const float3 tangent_in,
    const float2 texcoord_in,
    const half4 color_in,
    const uint primitive_id_in
) {
    VertexOutput output;

    output.primitive_id = primitive_id_in;
    PrimitiveDataGPU data = primitive_datas[output.primitive_id];

    float4 viewspace_position = mul(camera_data.view, mul(data.model, float4(position_in, 1.f)));
    output.position = mul(camera_data.projection, viewspace_position);

    output.viewspace_position = viewspace_position.xyz; 
    output.normal = (half3)normalize(mul((half3x3)data.model, normal_in));
    output.tangent = (half3)normalize(mul((half3x3)data.model, tangent_in));
    output.texcoord = texcoord_in;
    output.color = color_in;

    return output;
}
