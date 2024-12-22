#pragma once

#include <ffx_api/ffx_api.h>
#include <FidelityFX/host/ffx_cacao.h>

#include "render/backend/handles.hpp"

class SceneTransform;
class RenderGraph;

/**
 * Renders ambient occlusion
 *
 * Initially just uses AMD's Contrast Adaptive Compute Ambient Occlusion. I may eventually add more options, such as
 * HBAO+ or ray traced AO
 *
 * https://github.com/nvpro-samples/gl_ssao
 *
 * Also consider https://www.activision.com/cdn/research/Practical_Real_Time_Strategies_for_Accurate_Indirect_Occlusion_NEW%20VERSION_COLOR.pdf 
 */
class AmbientOcclusionPhase {
public:
    AmbientOcclusionPhase();

    ~AmbientOcclusionPhase();

    void generate_ao(
        RenderGraph& graph, const SceneTransform& view, TextureHandle gbuffer_normals, TextureHandle gbuffer_depth,
        TextureHandle ao_out
    );

private:
    FfxInterface ffx_interface;

    ffxContext ffx = nullptr;
    FfxDevice ffx_device;
    bool has_context = false;
    FfxCacaoContext context = {};
};
