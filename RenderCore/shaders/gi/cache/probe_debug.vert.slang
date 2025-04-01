#include "shared/view_data.hpp"
#include "shared/gi_probe.hpp"

ConstantBuffer<ViewDataGPU> view_data;

ConstantBuffer<IrradianceProbeVolume> probe_volume;

[vk::push_constant]
cbuffer Constants {
    uint debug_mode;
    uint cascade_index;
};

struct VertexOutput {
    float4 position : SV_Position;
    half2 offset;
    u16vec3 probe_id;
};

[shader("vertex")]
VertexOutput main(uint vertex_id: SV_VertexID, uint instance_id: SV_InstanceID) {
    const uint3 probe_id = uint3(instance_id % 32, (instance_id / 32) % 8, instance_id / (32 * 8));
    const ProbeCascade cascade = probe_volume.cascades[cascade_index];
    const float3 worldspace_probe_position = cascade.min + (float3)probe_id * cascade.probe_spacing;
    const float4 viewspace_probe_position = mul(view_data.view, float4(worldspace_probe_position, 1));

    // Draw the probe with a radius of probe_spacing / 2 in viewspace;
    const float probe_radius = 0.0625;//cascade.probe_spacing / 8.f;

    float4 view_position = viewspace_probe_position;

    // I'm sure there's a better way to do this but brute force best force
    float2 vertex_offset = 0;
    switch (vertex_id) {
    case 0:
        vertex_offset = float2(-1, 1);
        break;
    case 1:
        vertex_offset = float2(-1, -1);
        break;
    case 2:
        vertex_offset = float2(1, -1);
        break;
    case 3:
        vertex_offset = float2(1, -1);
        break;
    case 4:
        vertex_offset = float2(1, 1);
        break;
    case 5:
        vertex_offset = float2(-1, 1);
        break;
    }

    view_position.xy += vertex_offset * probe_radius;

    VertexOutput output = (VertexOutput)0;
    output.position = mul(view_data.projection, view_position);
    output.offset = (half2)vertex_offset;
    output.probe_id = (u16vec3)probe_id;

    return output;
}
