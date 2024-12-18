#version 460

/**
 * Visualizes the VPLs in a given cascade
 */

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_viewport_layer_array : enable
#extension GL_EXT_buffer_reference_uvec2 : enable

#include "common/spherical_harmonics.glsl"
#include "shared/lpv.hpp"
#include "shared/vpl.hpp"

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer VplListBuffer {
     PackedVPL vpls[];
};

layout(push_constant) uniform Constants {
    VplListBuffer vpl_list_buffer;
    float vpl_size;
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

    color = vpl.color;
    normal = vpl.normal;

    gl_Position = vec4(vpl.position, 1.f);
}
