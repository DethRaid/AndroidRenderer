#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_viewport_layer_array : enable

#include "common/spherical_harmonics.glsl"
#include "shared/lpv.hpp"
#include "shared/vpl.hpp"

layout(set = 0, binding = 0) uniform LpvCascadeBuffer {
    LPVCascadeMatrices cascade_matrices[4];
};

layout(set = 0, binding = 1) readonly buffer VplListBuffer {
    PackedVPL vpls[];
};

layout(push_constant) uniform Constants {
    int cascade_index;
} push_constants;

layout(location = 0) out vec3 color;
layout(location = 1) out vec3 normal;

VPL unpack_vpl(PackedVPL packed_vpl) {
    VPL vpl;

    vec2 unpacked_y = unpackHalf2x16(packed_vpl.data.y);

    vpl.position.xy = unpackHalf2x16(packed_vpl.data.x);
    vpl.position.z = unpacked_y.y;
    vpl.color.rg = unpackHalf2x16(packed_vpl.data.z);
    vpl.color.b = unpacked_y.x;
    vpl.normal = normalize(unpackUnorm4x8(packed_vpl.data.w).xyz);

    return vpl;
}

void main() {
    PackedVPL packed_vpl = vpls[gl_VertexIndex];
    VPL vpl = unpack_vpl(packed_vpl);

    vec3 position = vec3(cascade_matrices[push_constants.cascade_index].world_to_cascade * vec4(vpl.position, 1.f));
    color = vpl.color;
    normal = vpl.normal;

    // Adjust position to be in the correct part of the cascade
    position.x += push_constants.cascade_index;
    position.x /= 4.0;  // TODO: NUM_CASCADES as push constant

    gl_Position = vec4(position.xy * 2.f - 1.f, 0.f, 1.f);
    gl_Layer = int(position.z * 32.f);
    gl_PointSize = 1.f;
}
