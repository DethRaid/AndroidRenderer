#ifndef VERTEX_DATA_HPP
#define VERTEX_DATA_HPP

#include "shared/prelude.h"

// TODO: Compress down to 20 bytes
// We're at 48 now :(
struct StandardVertex {
    float3 position;
    float3 normal;
    float3 tangent;
    float2 texcoord;
    uint color;
};

#if defined(__cplusplus)
using StandardVertexPosition = glm::vec3;
#endif

struct StandardVertexData {
    float3 normal;
    float3 tangent;
    float2 texcoord;
    unorm4 color;
};

#endif
