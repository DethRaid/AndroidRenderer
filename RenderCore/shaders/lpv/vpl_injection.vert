#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_viewport_layer_array : enable
#extension GL_EXT_buffer_reference_uvec2 : enable

#include "common/spherical_harmonics.glsl"
#include "shared/lpv.hpp"
#include "shared/vpl.hpp"

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer VplListBuffer {
     PackedVPL vpls[];
};

layout(set = 0, binding = 0) uniform LpvCascadeBuffer {
    LPVCascadeMatrices cascade_matrices[4];
};

layout(push_constant) uniform Constants {
    VplListBuffer vpl_list_buffer;
    uint cascade_index;
    uint num_cascades;
};

layout(location = 0) out mediump vec3 color;
layout(location = 1) out mediump vec3 normal;

VPL unpack_vpl(PackedVPL packed_vpl) {
    VPL vpl;

    mediump vec2 unpacked_x = unpackHalf2x16(packed_vpl.data.x);
    mediump vec2 unpacked_y = unpackHalf2x16(packed_vpl.data.y);
    mediump vec2 unpacked_z = unpackHalf2x16(packed_vpl.data.z);
    lowp vec4 unpacked_w = unpackSnorm4x8(packed_vpl.data.w);

    vpl.position.xy = unpacked_x.xy;
    vpl.position.z = unpacked_y.x;
    vpl.color.r = unpacked_y.y;
    vpl.color.gb = unpacked_z.xy;
    vpl.normal = normalize(unpacked_w.xyz);

    return vpl;
}

void main() {
    PackedVPL packed_vpl = vpl_list_buffer.vpls[gl_VertexIndex];
    VPL vpl = unpack_vpl(packed_vpl);

    vec3 position = vec3(cascade_matrices[cascade_index].world_to_cascade * vec4(vpl.position, 1.f));
    color = vpl.color;
    normal = vpl.normal;

    // Adjust position to be in the correct part of the cascade
    position.x += float(cascade_index);
    position.x /= float(num_cascades);

    gl_Position = vec4(position.xy * 2.f - 1.f, 0.f, 1.f);
    gl_Layer = int(position.z * 32.f);
    gl_PointSize = 1.f;
}
