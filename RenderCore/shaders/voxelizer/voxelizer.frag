#version 460

#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference_uvec2 : enable
#extension GL_EXT_shader_image_load_formatted : enable

#include "common/spherical_harmonics.glsl"
#include "shared/primitive_data.hpp"
#include "shared/basic_pbr_material.hpp"

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer FrustumMatricesBuffer {
    mat4 world_to_bounds;
};

layout(push_constant) uniform Constants {
    uint primitive_id;
};

layout(set = 0, binding = 0) uniform image3D voxels;
layout(set = 0, binding = 1) readonly buffer PrimitiveDataBuffer {
    PrimitiveDataGPU primitive_datas[];
};

layout(set = 0, binding = 2, scalar) readonly buffer MaterialDataBuffer {
    BasicPbrMaterialGpu materials[];
};

layout(set = 1, binding = 0) uniform sampler2D textures_rgba8[];

layout(location = 0) in vec3 position_in;
layout(location = 1) in mediump vec2 texcoord_in;
layout(location = 2) in mediump vec4 color_in;

void main() {
    PrimitiveDataGPU primitive = primitive_datas[primitive_id];
    BasicPbrMaterialGpu material = materials[primitive.material_id];

    mediump vec4 base_color_sample = texture(textures_rgba8[nonuniformEXT(material.base_color_texture_index)], texcoord_in);
    mediump vec4 tinted_base_color = base_color_sample * material.base_color_tint * color_in;

    ivec3 dimensions = imageSize(voxels);
    vec3 voxel_position = (position_in * 0.5 + 0.5) * dimensions;
    ivec3 texel = ivec3(round(voxel_position));

    // In theory, the GPU will only rasterize one triangle for each output pixel and there won't be issues with overwriting data. In practice, this is probably UB
    imageStore(voxels, texel, tinted_base_color);
}

// I'll try spinning, that's a good trick!
// Except on Mali where it crashes :(
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
