#version 460

#extension GL_EXT_scalar_block_layout : enable
#extension GL_KHR_shader_subgroup_basic : enable
#extension GL_KHR_shader_subgroup_ballot : enable
#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_GOOGLE_include_directive : enable

#include "shared/vpl.hpp"
#include "shared/sun_light_constants.hpp"
#include "shared/lpv.hpp"

/**
 * Super cool fragment shader to extract VPLs from the RSM textures
 *
 * This is expected to run on a 1024x1024 render target, on a device with a subgroup size of 16. My
 * phone has such a subgroup size. Nvidia has a size of 32, AMD has a size of 64. On those devices
 * we may need to do something else to get the LPV quality we want... but that's a problem for later
 *
 * This FS doesn't output to a render target, that'd be cringe. Instead it atmonically adds lights
 * to a UAV
 *
 * This is a FS so we can keep the RSM gbuffers in tile memory. Vulkan isn't powerful enough to let
 * compute shaders read from tiled mem
 */

layout(set = 0, binding = 0, input_attachment_index = 0) uniform subpassInput rsm_flux;
layout(set = 0, binding = 1, input_attachment_index = 1) uniform subpassInput rsm_normal;
layout(set = 0, binding = 2, input_attachment_index = 2) uniform subpassInput rsm_depth;

layout(set = 0, binding = 3, std430) uniform LPVCascadesBuffer {
    LPVCascadeMatrices cascade_matrices[4];
} cascade_matrices_buffer;

layout(std430, set = 0, binding = 4) buffer CountBuffer {
    uint next_vpl_index;
};
layout(std430, set = 0, binding = 5) writeonly buffer VplListBuffer {
    PackedVPL lights[];
};

layout(push_constant) uniform Constants {
    int cascade_index;
} push_constants;

layout(location = 0) in vec2 texcoord;

vec4 get_worldspace_position() {
    float depth = subpassLoad(rsm_depth).r;
    vec2 texcoord = gl_FragCoord.xy / 1024.f;
    vec4 ndc_position = vec4(vec3(texcoord * 2.0 - 1.0, depth), 1.f);
    vec4 worldspace_position = cascade_matrices_buffer.cascade_matrices[push_constants.cascade_index].inverse_rsm_vp * ndc_position;
    worldspace_position /= worldspace_position.w;

    return worldspace_position;
}

void store_light(in VPL light) {
    uint light_index = atomicAdd(next_vpl_index, 1);

    PackedVPL packed_light;
    packed_light.data.x = packHalf2x16(light.position.xy);
    packed_light.data.y = packHalf2x16(vec2(0, light.position.z));
    packed_light.data.z = packUnorm4x8(vec4(light.color, 0));
    packed_light.data.w = packSnorm4x8(vec4(light.normal, 0));

    lights[light_index] = packed_light;
}

void main() {
    // Load this VPL, calculate it's luma
    VPL light;
    light.position = get_worldspace_position().xyz;
    light.color = subpassLoad(rsm_flux).rgb;
    light.normal = subpassLoad(rsm_normal).rgb * 2.f - 1.f;

    float luma = dot(light.color.rgb, vec3(0.2126, 0.7152, 0.0722));

    subgroupBarrier();
    float max_luma = subgroupInclusiveMax(luma);

    uvec4 result = subgroupBallot(max_luma == luma);
    uint first_max_thread = subgroupBallotFindLSB(result);

    if(first_max_thread == gl_SubgroupInvocationID) {
        // TODO: Store this light in the cascade's VPL linked list. Then there's no need to store
        // the VPL position, and we can SMASH that data usage!!!!
        store_light(light);
    }
}