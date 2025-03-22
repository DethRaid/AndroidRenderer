#ifndef VIEW_INFO_HPP
#define VIEW_INFO_HPP

#include "shared/prelude.h"

struct ViewInfo {
    float4x4 view;
    float4x4 projection;

    float4x4 inverse_view;
    float4x4 inverse_projection;

    float4 render_resolution;
};

#endif
