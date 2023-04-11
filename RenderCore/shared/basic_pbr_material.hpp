#ifndef BASIC_PBR_MATERIAL_HPP
#define BASIC_PBR_MATERIAL_HPP

#include "shared/prelude.h"

struct BasicPbrMaterialGpu {
    vec4 base_color_tint;
    vec4 emission_factor;
    float metalness_factor;
    float roughness_factor;

    vec2 padding0;
    vec4 padding1;
};

#endif
