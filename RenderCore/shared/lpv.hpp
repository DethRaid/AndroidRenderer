#ifndef LPV_HPP
#define LPV_HPP

#include "shared/prelude.h"

struct LPVCascadeMatrices {
    mat4 rsm_vp;
    mat4 inverse_rsm_vp;
    mat4 world_to_cascade;
};

#endif
