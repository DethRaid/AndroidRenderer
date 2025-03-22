#ifndef BASIC_PBR_MATERIAL_HPP
#define BASIC_PBR_MATERIAL_HPP

#include "shared/prelude.h"

struct BasicPbrMaterialGpu {
    float4 base_color_tint;
    float4 emission_factor;
    float metalness_factor;
    float roughness_factor;

    float opacity_threshold;
    float padding1;

    uint base_color_texture_index;
    uint normal_texture_index;
    uint data_texture_index;
    uint emission_texture_index;
};

#endif
