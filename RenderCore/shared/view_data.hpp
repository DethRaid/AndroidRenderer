#ifndef VIEW_DATA_GPU_HPP
#define VIEW_DATA_GPU_HPP

#include "shared/prelude.h"

struct ViewDataGPU {
    mat4 view;
    mat4 projection;

    mat4 inverse_view;
    mat4 inverse_projection;

    mat4 last_frame_view;
    mat4 last_frame_projection;

    /**
     * For perspective cameras, this is the frustum clip planes - xy stores the x and z of the right plane's equation,
     * zw stores the y and z of the top plane's equation
     *
     * For ortho cameras, xy stores the x and w of the right clip plane's plane equation, and zw stores the y and w of
     * the top plane's plane equation
     */
    vec4 frustum;

    /**
     * Near clipping plane
     */
    float z_near;

    /**
     * Bias to apply to material textures, useful for TAA
     */
    float material_texture_mip_bias;
    
    vec2 render_resolution;

    vec2 jitter;

    vec2 previous_jitter;
};

#endif
