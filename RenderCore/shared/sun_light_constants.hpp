#ifndef SUN_LIGHT_CONSTANTS_HPP
#define SUN_LIGHT_CONSTANTS_HPP

#include "shared/prelude.h"

struct SunLightConstants {
    vec4 direction_and_size;
    vec4 color;

    uvec4 csm_resolution;

    // Split depth in the x, unused in the y z and w
    vec4 data[4];

    /**
     * Matrix that goes from world space -> shadow NDC
     */
    mat4 cascade_matrices[4];

    /**
     * Matrix that goes from shadow NDC -> world space
     */
    mat4 cascade_inverse_matrices[4];
};

#endif
