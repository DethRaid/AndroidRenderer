#ifndef PRIMITIVE_DATA_HPP
#define PRIMITIVE_DATA_HPP


#include "shared/prelude.h"

#if defined(GL_core_profile)
#extension GL_EXT_shader_16bit_storage : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#endif

#define PRIMITIVE_TYPE_SOLID 0
#define PRIMITIVE_TYPE_CUTOUT 1
#define PRIMITIVE_TYPE_TRANSPARENT 2

struct PrimitiveDataGPU {
    mat4 model;
    mat4 inverse_model;

    // Bounds min (xyz) and radius (w) of the mesh
    vec4 bounds_min_and_radius;
    vec4 bounds_max;

    uint material_id;
    
    uint padding;

    uint mesh_id;
    uint type;  // See the PRIMITIVE_TYPE_ defines above

    uint voxels_color_srv;
    uint voxels_normal_srv;
    u16vec2 voxel_size_xy;
    u16vec2 voxel_size_zw;
};

#endif
