#include "material_pipelines.hpp"

#include "backend/render_backend.hpp"

MaterialPipelines::MaterialPipelines(std::string_view material_name) {
#if TRACY_ENABLE
    const auto material_name_c = fmt::format("MaterialPipelines({})", material_name);
    ZoneTransientN(_compile_materials, material_name_c.c_str(), true);
#endif

    auto& backend = RenderBackend::get();

    {
        const auto variant_name = fmt::format("{}_prepass", material_name);
        depth_pso = backend.begin_building_pipeline(variant_name)
                           .set_vertex_shader(fmt::format("shaders/materials/{}.vert.spv", variant_name))
                           .enable_dgc()
                           .build();
    }

    {
        const auto variant_name = fmt::format("{}_prepass_masked", material_name);
        depth_masked_pso = backend.begin_building_pipeline(variant_name)
                                  .set_vertex_shader(fmt::format("shaders/materials/{}.vert.spv", variant_name))
                                  .set_fragment_shader(fmt::format("shaders/materials/{}.frag.spv", variant_name))
                                  .enable_dgc()
                                  .build();
    }

    {
        const auto variant_name = fmt::format("{}_shadow", material_name);
        shadow_pso = backend.begin_building_pipeline(variant_name)
                            .set_vertex_shader(fmt::format("shaders/materials/{}.vert.spv", variant_name))
                            .set_raster_state(
                                {
                                    .depth_clamp_enable = true
                                }
                            )
                            .build();
    }

    {
        const auto variant_name = fmt::format("{}_shadow_masked", material_name);
        shadow_masked_pso = backend.begin_building_pipeline(variant_name)
                                   .set_vertex_shader(fmt::format("shaders/materials/{}.vert.spv", variant_name))
                                   .set_fragment_shader(fmt::format("shaders/materials/{}.frag.spv", variant_name))
                                   .set_raster_state(
                                       {
                                           .depth_clamp_enable = true
                                       }
                                   )
                                   .build();
    }

    constexpr auto blend_state = VkPipelineColorBlendAttachmentState{
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT
    };
    {
        const auto variant_name = fmt::format("{}_rsm", material_name);
        rsm_pso = backend.begin_building_pipeline(variant_name)
                         .set_vertex_shader(fmt::format("shaders/materials/{}.vert.spv", variant_name))
                         .set_fragment_shader(fmt::format("shaders/materials/{}.frag.spv", variant_name))
                         .set_blend_state(0, blend_state)
                         .set_blend_state(1, blend_state)
                         .build();
    }

    {
        const auto variant_name = fmt::format("{}_rsm_masked", material_name);
        rsm_masked_pso = backend.begin_building_pipeline(variant_name)
                                .set_vertex_shader(fmt::format("shaders/materials/{}.vert.spv", variant_name))
                                .set_fragment_shader(fmt::format("shaders/materials/{}.frag.spv", variant_name))
                                .set_blend_state(0, blend_state)
                                .set_blend_state(1, blend_state)
                                .build();
    }

    {
        const auto variant_name = fmt::format("{}_gbuffer", material_name);
        gbuffer_pso = backend.begin_building_pipeline(variant_name)
                              .set_vertex_shader(fmt::format("shaders/materials/{}.vert.spv", variant_name))
                              .set_fragment_shader(fmt::format("shaders/materials/{}.frag.spv", variant_name))
                              .set_depth_state(
                                  {
                                      .enable_depth_test = true,
                                      .enable_depth_write = false,
                                      .compare_op = VK_COMPARE_OP_EQUAL
                                  }
                              )
                              .set_blend_state(0, blend_state)
                              .set_blend_state(1, blend_state)
                              .set_blend_state(2, blend_state)
                              .set_blend_state(3, blend_state)
                              .build();
    }
    {
        const auto variant_name = fmt::format("{}_gbuffer_masked", material_name);
        gbuffer_masked_pso = backend.begin_building_pipeline(variant_name)
                                     .set_vertex_shader(fmt::format("shaders/materials/{}.vert.spv", variant_name))
                                     .set_fragment_shader(fmt::format("shaders/materials/{}.frag.spv", variant_name))
                                     .set_depth_state(
                                         {
                                             .enable_depth_test = true,
                                             .enable_depth_write = false,
                                             .compare_op = VK_COMPARE_OP_EQUAL
                                         }
                                     )
                                     .set_blend_state(0, blend_state)
                                     .set_blend_state(1, blend_state)
                                     .set_blend_state(2, blend_state)
                                     .set_blend_state(3, blend_state)
                                     .build();
    }

    //transparent_pso = backend.begin_building_pipeline("transparent")
    //                         .set_vertex_shader("shaders/deferred/basic.vert.spv")
    //                         .set_fragment_shader("shaders/deferred/standard_pbr.frag.spv")
    //                         .set_blend_state(
    //                             0,
    //                             {
    //                                 .blendEnable = VK_TRUE,
    //                                 .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    //                                 .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    //                                 .colorBlendOp = VK_BLEND_OP_ADD,
    //                                 .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    //                                 .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    //                                 .alphaBlendOp = VK_BLEND_OP_ADD,
    //                                 .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
    //                                 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    //                             })
    //                         .build();

}

GraphicsPipelineHandle MaterialPipelines::get_depth_pso() const {
    return depth_pso;
}

GraphicsPipelineHandle MaterialPipelines::get_depth_masked_pso() const {
    return depth_masked_pso;
}

GraphicsPipelineHandle MaterialPipelines::get_shadow_pso() const {
    return shadow_pso;
}

GraphicsPipelineHandle MaterialPipelines::get_shadow_masked_pso() const {
    return shadow_masked_pso;
}

GraphicsPipelineHandle MaterialPipelines::get_rsm_pso() const {
    return rsm_pso;
}

GraphicsPipelineHandle MaterialPipelines::get_rsm_masked_pso() const {
    return rsm_masked_pso;
}

GraphicsPipelineHandle MaterialPipelines::get_gbuffer_pso() const {
    return gbuffer_pso;
}

GraphicsPipelineHandle MaterialPipelines::get_gbuffer_masked_pso() const {
    return gbuffer_masked_pso;
}
