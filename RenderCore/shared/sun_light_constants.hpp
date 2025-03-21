#ifndef SUN_LIGHT_CONSTANTS_HPP
#define SUN_LIGHT_CONSTANTS_HPP

#include "shared/prelude.h"

#define SHADOW_MODE_OFF 0
#define SHADOW_MODE_CSM 1
#define SHADOW_MODE_RT  2

struct SunLightConstants {
    /**
     * Light direction (xyz) and tangent of the angular size (w)
     */
    vec4 direction_and_tan_size;

    /**
     * HDR light color
     */
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

    /**
     * How to handle this light's shadows. See SHADOW_MODE_ above
     */
    uint shadow_mode;

    float num_shadow_samples;
    uint padding1;
    uint padding2;
};

#endif
