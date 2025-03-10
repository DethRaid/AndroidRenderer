#pragma once

#if SAH_USE_FFX
#include <FidelityFX/host/ffx_cacao.h>
#endif

#include "render/backend/handles.hpp"

struct NoiseTexture;
class RenderScene;
class SceneView;
class RenderGraph;

enum class AoTechnique {
    Off,
    CACAO,
    RTAO
};

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
        RenderGraph& graph, const SceneView& view, const RenderScene& scene, const NoiseTexture& noise,
        TextureHandle gbuffer_normals, TextureHandle gbuffer_depth, TextureHandle ao_out
    );

private:
#if SAH_USE_FFX
    FfxInterface ffx_interface;

    FfxDevice ffx_device;
    bool has_context = false;
    FfxCacaoContext context = {};

    TextureHandle stinky_depth = nullptr;
#endif

    void evaluate_cacao(
        RenderGraph& graph, const SceneView& view, TextureHandle gbuffer_depth, TextureHandle gbuffer_normals,
        TextureHandle ao_out
    );

    ComputePipelineHandle rtao_pipeline = nullptr;

    uint32_t frame_index = 0;

    void evaluate_rtao(
        RenderGraph& graph, const SceneView& view, const RenderScene& scene, const NoiseTexture& noise,
        TextureHandle gbuffer_depth, TextureHandle gbuffer_normals, TextureHandle ao_out
    );
};
