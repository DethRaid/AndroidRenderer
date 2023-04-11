#ifndef PRIMITIVE_DATA_HPP
#define PRIMITIVE_DATA_HPP

#include "shared/prelude.h"

struct PrimitiveDataGPU {
    mat4 model;
    mat4 inverse_model;

    // Material index in the X
    uvec4 data; 
};

#endif
