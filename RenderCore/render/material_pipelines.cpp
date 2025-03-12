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
        .enable_dgc()
        .build();

    depth_masked_pso = backend.begin_building_pipeline("depth_prepass_masked")
        .set_vertex_shader("shaders/deferred/basic.vert.spv")
        .set_fragment_shader("shaders/prepass/masked.frag.spv")
        .enable_dgc()
        .build();

    shadow_pso = backend.begin_building_pipeline("shadow")
        .set_vertex_shader("shaders/lighting/shadow.vert.spv")
        .set_raster_state(
            {
                .depth_clamp_enable = true
            }
        )
        .build();

    shadow_masked_pso = backend.begin_building_pipeline("shadow_masked")
        .set_vertex_shader("shaders/lighting/shadow_masked.vert.spv")
        .set_fragment_shader("shaders/prepass/masked.frag.spv")
        .set_raster_state(
            {
                .depth_clamp_enable = true
            }
        )
        .build();


    sky_shadow_pso = backend.begin_building_pipeline("sky_shadow")
        .set_vertex_shader("shaders/lighting/sky_shadow.vert.spv")
        .set_raster_state(
            {
                .depth_clamp_enable = true
            }
        )
        .build();

    sky_shadow_masked_pso = backend.begin_building_pipeline("sky_shadow_masked")
        .set_vertex_shader("shaders/lighting/sky_shadow_masked.vert.spv")
        .set_fragment_shader("shaders/prepass/masked.frag.spv")
        .set_raster_state(
            {
                .depth_clamp_enable = true
            }
        )
        .build();

    constexpr auto blend_state = VkPipelineColorBlendAttachmentState{
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT
    };
    rsm_pso = backend.begin_building_pipeline("rsm")
        .set_vertex_shader("shaders/lpv/rsm.vert.spv")
        .set_fragment_shader("shaders/lpv/rsm.frag.spv")
        .set_blend_state(0, blend_state)
        .set_blend_state(1, blend_state)
        .build();

    rsm_masked_pso = backend.begin_building_pipeline("rsm_masked")
        .set_vertex_shader("shaders/lpv/rsm.vert.spv")
        .set_fragment_shader("shaders/lpv/rsm_masked.frag.spv")
        .set_blend_state(0, blend_state)
        .set_blend_state(1, blend_state)
        .build();
}

GraphicsPipelineHandle MaterialPipelines::get_depth_pso() const {
    return depth_pso;
}

GraphicsPipelineHandle MaterialPipelines::get_depth_masked_pso() const { return depth_masked_pso; }

GraphicsPipelineHandle MaterialPipelines::get_shadow_pso() const {
    return shadow_pso;
}

GraphicsPipelineHandle MaterialPipelines::get_shadow_masked_pso() const {
    return shadow_masked_pso;
}

GraphicsPipelineHandle MaterialPipelines::get_sky_shadow_pso() const { return sky_shadow_pso; }
GraphicsPipelineHandle MaterialPipelines::get_sky_shadow_masked_pso() const { return sky_shadow_masked_pso; }

GraphicsPipelineHandle MaterialPipelines::get_rsm_pso() const { return rsm_pso; }

GraphicsPipelineHandle MaterialPipelines::get_rsm_masked_pso() const { return rsm_masked_pso; }

GraphicsPipelineHandle MaterialPipelines::get_gbuffers_pso() const { return gbuffers_pso; }
GraphicsPipelineHandle MaterialPipelines::get_gbuffers_masked_pso() const { return gbuffers_masked_pso; }

GraphicsPipelineHandle MaterialPipelines::get_transparent_pso() const { return transparent_pso; }
