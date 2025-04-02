#version 460

#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#include "shared/prelude.h"
#include "shared/lpv.hpp"

layout(points) in;
layout(points, max_vertices = 4) out;

layout(set = 0, binding = 2, scalar) uniform LpvCascadeBuffer {
    LPVCascadeMatrices cascade_matrices[4];
};

layout(push_constant) uniform Constants {
    uint resolution_x;
    uint resolution_y;
    uint num_cascades;
};

layout(location = 0) in mediump vec4 sh_in[1];

layout(location = 0) out mediump vec4 sh_out;

void main() {
    for(uint cascade_index = 0; cascade_index < num_cascades; cascade_index++) {
        vec4 cascade_position = cascade_matrices[cascade_index].world_to_cascade * gl_in[0].gl_Position;

        if(any(lessThan(cascade_position.xyz, vec3(0))) || any(greaterThan(cascade_position.xyz, vec3(1)))) {
            gl_Position = vec4(-1);
        } else {    
            gl_Position = vec4(cascade_position.xy, 0, 1);
            gl_Position.x += cascade_index;
            gl_Position.x /= num_cascades;
        }

        gl_Layer = int(cascade_position.z * 32.f);
        sh_out = sh_in[0];

        EmitVertex();
    }
}

