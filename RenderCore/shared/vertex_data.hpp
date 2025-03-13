#ifndef VERTEX_DATA_HPP
#define VERTEX_DATA_HPP

#include "shared/prelude.h"

// TODO: Compress down to 20 bytes
// We're at 48 now :(
struct StandardVertex {
    vec3 position;
    vec3 normal;
    vec3 tangent;
    vec2 texcoord;
    uint color;
};

#if defined(__cplusplus)
using VertexPosition = glm::vec3;
#endif

struct StandardVertexData {
    vec3 normal;
    vec3 tangent;
    vec2 texcoord;
    uint color;
};

#endif
