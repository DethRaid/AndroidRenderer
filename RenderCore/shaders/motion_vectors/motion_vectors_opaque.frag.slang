#include "shared/primitive_data.hpp"
#include "shared/view_data.hpp"

[[vk::binding(0, 0)]]
ConstantBuffer<ViewDataGPU> camera_data;

[[vk::binding(1, 0)]]
StructuredBuffer<PrimitiveDataGPU> primitive_datas;

struct VertexOutput {
    float4 position : SV_Position;
    float3 previous_clipspace_location : PREVIOUS_CLIPSPACE_LOCATION;
    uint primitive_id : PRIMITIVE_ID;
};

[shader("fragment")]
float2 main(VertexOutput vertex) {
    const float2 previous_clipspace_location = vertex.previous_clipspace_location.xy / vertex.previous_clipspace_location.z;
    const float2 previous_uv_location = previous_clipspace_location * float2(0.5, 0.5) + 0.5;
    const float2 previous_pixel_location = previous_uv_location * camera_data.render_resolution;

    const float2 pixel_motion_vectors = previous_pixel_location - vertex.position.xy;

    return pixel_motion_vectors;
}
