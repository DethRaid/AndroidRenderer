#ifndef VOXELIZER_COMPUTE_PASS_PARAMETERS_HPP
#define VOXELIZER_COMPUTE_PASS_PARAMETERS_HPP

#include "shared/prelude.h"

struct VoxelizerComputePassParameters {
    vec4 bounds_min;
    float half_cell_size;
    uint primitive_index;
};

#endif
