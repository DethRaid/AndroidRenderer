#ifndef LPV_HPP
#define LPV_HPP

#include "shared/prelude.h"

struct LPVCascadeMatrices {
    float4x4 rsm_vp;
    float4x4 inverse_rsm_vp;
    float4x4 world_to_cascade;
    float4x4 cascade_to_world;
};

#endif
