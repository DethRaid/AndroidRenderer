#include "lighting_phase.hpp"

#include "render/render_scene.hpp"
#include "render/scene_view.hpp"
#include "shared/view_data.hpp"

LightingPhase::LightingPhase() {
    auto& backend = RenderBackend::get();
    emission_pipeline = backend.begin_building_pipeline("Emissive Lighting")
                               .set_vertex_shader("shaders/common/fullscreen.vert.spv")
                               .set_fragment_shader("shaders/lighting/emissive.frag.spv")
                               .set_depth_state(
                                   DepthStencilState{.enable_depth_test = VK_FALSE, .enable_depth_write = VK_FALSE}
                               )
                               .set_blend_state(
                                   0,
                                   {
                                       .blendEnable = VK_TRUE,
                                       .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                       .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                       .colorBlendOp = VK_BLEND_OP_ADD,
                                       .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                       .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                       .alphaBlendOp = VK_BLEND_OP_ADD,
                                       .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                       VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                                   }
                               )
                               .build();
}

void LightingPhase::render(
    RenderGraph& render_graph, const SceneTransform& view, const TextureHandle lit_scene_texture,
    const LightPropagationVolume* lpv
) const {
    ZoneScoped;

    if(scene == nullptr) {
        return;
    }

    auto& backend = RenderBackend::get();

    auto& sun = scene->get_sun_light();
    auto& sun_pipeline = sun.get_pipeline();

    const auto sampler = backend.get_default_sampler();
    auto gbuffers_descriptor_set = backend.get_transient_descriptor_allocator().build_set(sun_pipeline, 0)
                                          .bind(0, gbuffer.color, sampler)
                                          .bind(1, gbuffer.normal, sampler)
                                          .bind(2, gbuffer.data, sampler)
                                          .bind(3, gbuffer.emission, sampler)
                                          .bind(4, gbuffer.depth, sampler)
                                          .build();

    auto texture_usages = absl::flat_hash_map<TextureHandle, TextureUsageToken>{};
    const auto sun_shadowmap_handle = sun.get_shadowmap_handle();
    if(sun_shadowmap_handle != TextureHandle::None) {
        texture_usages.emplace(
            sun.get_shadowmap_handle(),
            TextureUsageToken{
                .stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, .access = VK_ACCESS_2_SHADER_READ_BIT,
                .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            });
    }

    render_graph.add_render_pass(
        {
            .name = "Lighting",
            .textures = texture_usages,
            .descriptor_sets = {gbuffers_descriptor_set},
            .color_attachments = {
                RenderingAttachmentInfo{.image = lit_scene_texture, .load_op = VK_ATTACHMENT_LOAD_OP_CLEAR}
            },
            .execute = [&](CommandBuffer& commands) {
                AccelerationStructureHandle rtas = {};
                if(RenderBackend::get().use_ray_tracing()) {
                    rtas = scene->get_raytracing_scene().get_acceleration_structure();
                }

                sun.render(commands, gbuffers_descriptor_set, view, rtas);

                if(lpv) {
                    lpv->add_lighting_to_scene(commands, gbuffers_descriptor_set, view.get_buffer());
                }

                if(*CVarSystem::Get()->GetIntCVar("r.MeshLight.Raytrace")) {
                    add_raytraced_mesh_lighting(commands, gbuffers_descriptor_set, view.get_buffer());
                }

                add_emissive_lighting(commands, gbuffers_descriptor_set);
            }
        });


}

void LightingPhase::set_gbuffer(const GBuffer& gbuffer_in) {
    gbuffer = gbuffer_in;
}


void LightingPhase::set_scene(RenderScene& scene_in) {
    scene = &scene_in;
}

void LightingPhase::add_raytraced_mesh_lighting(
    CommandBuffer& commands, const DescriptorSet& gbuffers_descriptor_set, BufferHandle view_buffer
) const {
    ZoneScoped;

    auto& sun = scene->get_sun_light();
    auto& raytracing_scene = scene->get_raytracing_scene();
}

void LightingPhase::add_emissive_lighting(CommandBuffer& commands, const DescriptorSet& gbuffer_descriptor_set) const {
    ZoneScoped;

    commands.begin_label("Emissive Lighting");

    commands.bind_pipeline(emission_pipeline);

    commands.bind_descriptor_set(0, gbuffer_descriptor_set);

    commands.draw_triangle();

    commands.clear_descriptor_set(0);

    commands.end_label();
}
