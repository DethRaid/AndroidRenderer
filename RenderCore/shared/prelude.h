#ifndef PRELUDE_H
#define PRELUDE_H

#if defined(__cplusplus)

// Typedefs so we can share structs between C++ and GLSL

#include <glm/glm.hpp>

using unorm4 = uint32_t;

using u16vec2 = glm::u16vec2;

using uint = uint32_t;
using uvec2 = glm::uvec2;
using uvec4 = glm::uvec4;

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;

#elif defined(GL_core_profile)

// A couple useful things for GLSL

#ifndef PI
#define PI 3.1415927
#endif

// Bad
#define unorm4 uint

#else
#define unorm4 unorm float4

#define u16vec2 uint16_t2

#define uvec2 uint2
#define uvec4 uint4

#define vec2 float2
#define vec3 float3
#define vec4 float4
#define mat4 float4x4
#endif

#endif

