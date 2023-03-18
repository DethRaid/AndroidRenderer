#ifndef VERTEX_DATA_HPP
#define VERTEX_DATA_HPP

#include "shared/prelude.h"

#if __cplusplus
using VertexPosition = glm::vec3;
#endif

struct StandardVertexData {
    vec3 normal;
    vec3 tangent;
    vec2 texcoord;
    uint color;
};

#endif
