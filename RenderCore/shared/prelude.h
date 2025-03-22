#ifndef PRELUDE_H
#define PRELUDE_H

#if defined(__cplusplus)

// Typedefs so we can share structs between C++, GLSL, and Slang

#include <glm/gtx/compatibility.hpp>

using unorm4 = uint32_t;

using u16vec2 = glm::u16vec2;

using uint = uint32_t;
using uint2 = glm::uvec2;
using uint4 = glm::uvec4;

using glm::float2;
using glm::float3;
using glm::float4;
using glm::float4x4;

#elif defined(GL_core_profile)

// A couple useful things for GLSL

// Bad
#define unorm4 uint

// useful
#define uint2 uvec2
#define uint4 uvec4

#define float2 vec2
#define float3 vec3
#define float4 vec4
#define float4x4 mat4

#else
#define unorm4 uint

#define u16vec2 uint16_t2
#endif

#ifndef PI
#define PI 3.1415927
#endif

#endif
