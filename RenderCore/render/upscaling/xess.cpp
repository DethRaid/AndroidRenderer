#include "xess.hpp"

#include "console/cvars.hpp"

static AutoCVar_Enum cvar_xess_mode{
    "r.XeSS.Mode",
    "XeSS quality mode\n\t100 = Ultra Performance (33% render scale)\n\t101 = Performance (43% render scale)\n\t102 = Balanced (50% render scale)\n\t103 = Quality (59% render scale)\n\t104 = Ultra Quality (66% render scale)\n\t105 = Ultra Quality Plus (77% render scale)\n\t106 = Native-res Anti-Aliasing",
    XESS_QUALITY_SETTING_PERFORMANCE
};

XeSSAdapter::~XeSSAdapter() {}

void XeSSAdapter::initialize(glm::uvec2 output_resolution, uint32_t frame_index) {}

glm::uvec2 XeSSAdapter::get_optimal_render_resolution() const { return {}; }

void XeSSAdapter::set_constants(const SceneView& scene_view, glm::uvec2 render_resolution) {}

void XeSSAdapter::evaluate(
    RenderGraph& graph, TextureHandle color_in, TextureHandle color_out, TextureHandle depth_in,
    TextureHandle motion_vectors_in
) {}
