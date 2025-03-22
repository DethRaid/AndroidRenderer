#ifndef VPL_HPP
#define VPL_HPP

#include "shared/prelude.h"

#if defined(__cplusplus)
#define mvec3 glm::mediump_vec3
#else
#define mvec3 mediump vec3
#endif

/**
 * A Virtual Point Light, packed into a single vec4
 *
 * x = xy of the position, halfs packed into a uint
 * y = b of the color and z of the position, halfs packed into a uint
 * z = rg color, hafls packed into a uint
 * w = normal, stored as snorm4
 *
 * Note that more clever normal encoding schemes are readily available. Storing it in two halfs would be great
 */
struct PackedVPL {
    uint4 data;
};

struct VPL {
    float3 position;
    mvec3 color;
    mvec3 normal;
};

#endif
