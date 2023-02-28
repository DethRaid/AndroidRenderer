#version 460

#extension GL_GOOGLE_include_directive : enable

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

layout(location = 0) out vec3 position;
layout(location = 1) out vec3 color;
layout(location = 2) out vec3 normal;

VPL unpack_vpl(PackedVPL packed_vpl) {
    VPL vpl;

    vpl.position.xy = unpackHalf2x16(packed_vpl.data.x);
    vpl.position.z = unpackHalf2x16(packed_vpl.data.y).x;
    vpl.color = unpackUnorm4x8(packed_vpl.data.z).rgb;
    vpl.normal = normalize(unpackSnorm4x8(packed_vpl.data.w).xyz);

    return vpl;
}

void main() {
    PackedVPL packed_vpl = vpls[gl_VertexIndex];
    VPL vpl = unpack_vpl(packed_vpl);

    position = vec3(cascade_matrices[push_constants.cascade_index].world_to_cascade * vec4(vpl.position, 1.f));
    color = vpl.color;
    normal = vpl.normal;

    gl_Position = vec4(position, 1.f);

    gl_PointSize = 1.f;
}
