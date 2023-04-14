#ifndef PRELUDE_H
#define PRELUDE_H

#if defined(__cplusplus)

// Typedefs so we can share structs between C++ and GLSL

#include <glm/glm.hpp>

using uint = uint32_t;
using uvec4 = glm::uvec4;

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;

#endif

#else

// A couple useful things for GLSL

#define PI 3.1415927

#define medfloat mediump float
#define medvec2 mediump vec2
#define medvec3 mediump vec3
#define medvec4 mediump vec4

#endif

