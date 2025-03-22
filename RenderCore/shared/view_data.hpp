#ifndef VIEW_DATA_GPU_HPP
#define VIEW_DATA_GPU_HPP

#include "shared/prelude.h"

struct ViewDataGPU {
    float4x4 view;
    float4x4 projection;

    float4x4 inverse_view;
    float4x4 inverse_projection;

    float4x4 last_frame_view;
    float4x4 last_frame_projection;

    /**
     * For perspective cameras, this is the frustum clip planes - xy stores the x and z of the right plane's equation,
     * zw stores the y and z of the top plane's equation
     *
     * For ortho cameras, xy stores the x and w of the right clip plane's plane equation, and zw stores the y and w of
     * the top plane's plane equation
     */
    float4 frustum;

    /**
     * Near clipping plane
     */
    float z_near;

    /**
     * Bias to apply to material textures, useful for TAA
     */
    float material_texture_mip_bias;
    
    float2 render_resolution;

    float2 jitter;

    float2 previous_jitter;
};

#endif
