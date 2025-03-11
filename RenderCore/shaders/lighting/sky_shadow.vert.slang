#include "shared/primitive_data.hpp"
#include "shared/view_data.hpp"
#include "shared/vertex_data.hpp"

[[vk::binding(0, 0)]]
ConstantBuffer<ViewDataGPU> camera_data;

[[vk::binding(1, 0)]]
StructuredBuffer<PrimitiveDataGPU> primitive_datas;

struct VertexOutput {
    float4 position : SV_Position;
};

[shader("vertex")]
VertexOutput main(
    const float3 position_in,
    const uint primitive_id_in
) {
    VertexOutput output;

    PrimitiveDataGPU data = primitive_datas[primitive_id_in];

    float4 viewspace_position = mul(camera_data.view, mul(data.model, float4(position_in, 1.f)));
    output.position = mul(camera_data.projection, viewspace_position);

    return output;
}
