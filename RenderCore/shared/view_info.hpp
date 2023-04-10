#ifndef VIEW_INFO_HPP
#define VIEW_INFO_HPP

#include "shared/prelude.h"

struct ViewInfo {
    mat4 view;
    mat4 projection;

    mat4 inverse_view;
    mat4 inverse_projection;

    vec4 render_resolution;
};

#endif
