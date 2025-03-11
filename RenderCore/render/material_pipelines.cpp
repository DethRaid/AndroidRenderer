#include "material_pipelines.hpp"

#include "backend/render_backend.hpp"

static std::unique_ptr<MaterialPipelines> instance;

MaterialPipelines& MaterialPipelines::get() {
    if(!instance) {
        instance = std::make_unique<MaterialPipelines>();
    }

    return *instance;
}

MaterialPipelines::MaterialPipelines() {
    auto& backend = RenderBackend::get();

    depth_pso = backend.begin_building_pipeline("depth_prepass")
        .set_vertex_shader("shaders/deferred/basic.vert.spv")
        .set_raster_state(
            {
                .front_face = VK_FRONT_FACE_CLOCKWISE
            }
        )
        .enable_dgc()
        .build();

    shadow_pso = backend.begin_building_pipeline("shadow")
        .set_vertex_shader("shaders/lighting/shadow.vert.spv")
        .set_raster_state(
            {
                .cull_mode = VK_CULL_MODE_FRONT_BIT,
                .front_face = VK_FRONT_FACE_CLOCKWISE,
                .depth_clamp_enable = true
            }
        )
        .build();

}

GraphicsPipelineHandle MaterialPipelines::get_depth_pso() const {
    return depth_pso;
}

GraphicsPipelineHandle MaterialPipelines::get_shadow_pso() const {
    return shadow_pso;
}

GraphicsPipelineHandle MaterialPipelines::get_rsm_pso() const { return rsm_pso; }

GraphicsPipelineHandle MaterialPipelines::get_gbuffers_pso() const { return gbuffers_pso; }

GraphicsPipelineHandle MaterialPipelines::get_transparent_pso() const { return transparent_pso; }
