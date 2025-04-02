#ifndef PRIMITIVE_DATA_HPP
#define PRIMITIVE_DATA_HPP

#include "shared/prelude.h"

#define PRIMITIVE_TYPE_SOLID 0
#define PRIMITIVE_TYPE_CUTOUT 1
#define PRIMITIVE_TYPE_TRANSPARENT 2

#if defined(__cplusplus)
using MaterialPointer = uint64_t;
using IndexPointer = uint64_t;
using VertexPositionPointer = uint64_t;
using VertexDataPointer = uint64_t;

#elif defined(GL_core_profile)
#define MaterialPointer uvec2
#define IndexPointer uvec2
#define VertexPositionPointer uvec2
#define VertexDataPointer uvec2

#else
#include "shared/basic_pbr_material.hpp"
#include "shared/vertex_data.hpp"

#define MaterialPointer BasicPbrMaterialGpu*
#define IndexPointer uint*
#define VertexPositionPointer float3*
#define VertexDataPointer StandardVertexData*
#endif

struct PrimitiveDataGPU {
    float4x4 model;
    float4x4 inverse_model;

    // Bounds min (xyz) and radius (w) of the mesh
    float4 bounds_min_and_radius;
    float4 bounds_max;

    MaterialPointer material;

    uint mesh_id;
    uint type;  // See the PRIMITIVE_TYPE_ defines above

    IndexPointer indices;
    VertexPositionPointer vertex_positions;
    VertexDataPointer vertex_data;
};

#endif
