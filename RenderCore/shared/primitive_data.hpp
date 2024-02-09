#ifndef PRIMITIVE_DATA_HPP
#define PRIMITIVE_DATA_HPP


#include "shared/prelude.h"

#include "shared/basic_pbr_material.hpp"

#if defined(__cplusplus)
#define MATERIAL_BUFFER_REFERENCE uvec2
#else
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference_uvec2 : enable

layout(buffer_reference, scalar, buffer_reference_align = 16) readonly buffer MaterialDataBuffer {
    BasicPbrMaterialGpu material;
};

#define MATERIAL_BUFFER_REFERENCE MaterialDataBuffer

#endif

#define PRIMITIVE_TYPE_SOLID 0
#define PRIMITIVE_TYPE_CUTOUT 1
#define PRIMITIVE_TYPE_TRANSPARENT 2

struct PrimitiveDataGPU {
    mat4 model;
    mat4 inverse_model;

    vec4 bounding_sphere;

    MATERIAL_BUFFER_REFERENCE material_id;

    uint mesh_id;
    uint type;  // See the PRIMITIVE_TYPE_ defines above
};

#endif
