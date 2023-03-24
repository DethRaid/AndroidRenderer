#version 460

#extension GL_GOOGLE_include_directive : enable

#include "common/spherical_harmonics.glsl"

layout(location = 0) in vec3 position_in;
layout(location = 1) in vec3 normal_in;

// Scene SH
layout(set = 0, binding = 1, r32ui) uniform uimage3D voxels_xy;
layout(set = 0, binding = 2, r32ui) uniform uimage3D voxels_zw;

// Count of SHes
// layout(set = 0, binding = 2, r32ui) uniform uimage3D sample_counts;

uint packInt2x16(ivec2 value) {
    uint result = (value.x << 16) | (value.y & 0x0000FFFF) & 0x7FFFFFFF;
    if(value.x < 0) {
        result |= 0x80000000;
    }
    if(value.y < 0) {
        result |= 0x00008000;
    }
    return result;
}

void main() {
    ivec3 dimensions = imageSize(voxels_xy);
    vec3 voxel_position = (position_in * 0.5 + 0.5) * dimensions;
    ivec3 texel = ivec3(round(voxel_position));
    
    // We want the SH of the surface - the surface is solid in the opposite direction of the normal
    vec4 sh = dir_to_sh(-normal_in);

    uint xy = packHalf2x16(sh.xy);
    uint zw = packHalf2x16(sh.zw);

    imageStore(voxels_xy, texel, xy.xxxx);
    imageStore(voxels_zw, texel, zw.xxxx);

    // Funky math to convert the SH to 16-bit fixed-point
    // We chose a representation with 10 bits in front of the decimal point, 5 bits behind, and one bit for the sign. 
    // This will hopefully give enough range for all the SHes we want to combine, while still allowing a bit of precision
    // ivec4 sh_int = ivec4(round(sh * 8));

    // Convert XY to a uint, and ZW to a uint
    // uint xy = packInt2x16(sh_int.xy);
    // uint zw = packInt2x16(sh_int.zw);

    // We do a minor amount of overflow
    // imageAtomicAdd(voxels_xy, texel, xy);
    // imageAtomicAdd(voxels_zw, texel, zw);

    // imageAtomicAdd(sample_counts, texel, 1);
}


// bool spinning = true;
// while(spinning) {
//     // Try to write 1 to the data
//     uint old_data = imageAtomicCompSwap(locks, texel, 0, 1);
//     // If the pre-writing value was 0, then 1 must have been written. We have the lock, let's party
//     if(old_data == 0) {
//         // Party!
//         // Perform your work here
// 
//         // Unlock the lock
//         imageAtomicExchange(locks, texel, 0);
//         // Stop looping
//         spinning = false;
//     }
// }

