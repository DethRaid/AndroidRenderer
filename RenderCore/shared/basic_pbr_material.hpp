#ifndef BASIC_PBR_MATERIAL_HPP
#define BASIC_PBR_MATERIAL_HPP

#include "shared/prelude.h"

struct BasicPbrMaterialGpu {
    vec4 base_color_tint;
    vec4 emission_factor;
    float metalness_factor;
    float roughness_factor;

    vec2 padding0;

    uint base_color_texture_index;
    uint normal_texture_index;
    uint data_texture_index;
    uint emission_texture_index;
};

#endif
