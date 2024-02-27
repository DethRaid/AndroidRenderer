#include "lighting_phase.hpp"

#include "render/render_scene.hpp"
#include "render/scene_view.hpp"
#include "shared/view_data.hpp"

LightingPhase::LightingPhase(RenderBackend& backend_in) : backend{backend_in} {
    emission_pipeline = backend.begin_building_pipeline("Emissive Lighting")
                               .set_vertex_shader("shaders/common/fullscreen.vert.spv")
                               .set_fragment_shader("shaders/lighting/emissive.frag.spv")
                               .set_depth_state(
                                   DepthStencilState{.enable_depth_test = VK_FALSE, .enable_depth_write = VK_FALSE}
                               )
                               .set_blend_state(
                                   0, {
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

void LightingPhase::render(CommandBuffer& commands, const SceneTransform& view, const std::unique_ptr<LightPropagationVolume>& lpv) {
    if (scene == nullptr) {
        return;
    }

    GpuZoneScopedN(commands, "LightingPhase::render")

    auto gbuffers_descriptor_set = *vkutil::DescriptorBuilder::begin(
        backend, backend.get_transient_descriptor_allocator()
    )
           .bind_image(
               0,
               {.sampler = {}, .image = gbuffer.color, .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
               VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT
           )
           .bind_image(
               1,
               {.sampler = {}, .image = gbuffer.normal, .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
               VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT
           )
           .bind_image(
               2,
               {.sampler = {}, .image = gbuffer.data, .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
               VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT
           )
           .bind_image(
               3,
               {.sampler = {}, .image = gbuffer.emission, .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
               VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT
           )
           .bind_image(
               4,
               {.sampler = {}, .image = gbuffer.depth, .image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL},
               VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT
           )
           .build();

    add_sun_lighting(commands, gbuffers_descriptor_set, view);

    if (lpv) {
        lpv->add_lighting_to_scene(commands, gbuffers_descriptor_set, view.get_buffer());
    }

    if(*CVarSystem::Get()->GetIntCVar("r.MeshLight.Raytrace")) {
        add_raytraced_mesh_lighting(commands, gbuffers_descriptor_set, view.get_buffer());
    }

    add_emissive_lighting(commands, gbuffers_descriptor_set);
}

void LightingPhase::set_gbuffer(const GBuffer& gbuffer_in) {
    gbuffer = gbuffer_in;
}

void LightingPhase::set_shadowmap(const TextureHandle shadowmap_in) {
    shadowmap = shadowmap_in;
}

void LightingPhase::set_scene(RenderScene& scene_in) {
    scene = &scene_in;
}

void
LightingPhase::add_sun_lighting(
    CommandBuffer& commands, const VkDescriptorSet gbuffers_descriptor_set, const SceneTransform& view
) const {
    commands.begin_label("LightingPhase::add_sun_lighting");

    auto& allocator = backend.get_global_allocator();

    // Hardware PCF sampler
    auto sampler = allocator.get_sampler(
        VkSamplerCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .compareEnable = VK_TRUE,
            .compareOp = VK_COMPARE_OP_LESS,
            .minLod = 0,
            .maxLod = 16,
        }
    );

    auto& sun = scene->get_sun_light();
    auto& sun_pipeline = sun.get_pipeline();
    commands.bind_pipeline(sun_pipeline);

    const auto sun_buffer = sun.get_constant_buffer();

    commands.bind_descriptor_set(0, gbuffers_descriptor_set);

    auto sun_descriptor_set = *vkutil::DescriptorBuilder::begin(
        backend, backend.get_transient_descriptor_allocator()
    )
           .bind_image(
               0,
               {.sampler = sampler, .image = shadowmap, .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT
           )
           .bind_buffer(
               1, {.buffer = sun_buffer, .offset = 0, .range = sizeof(SunLightConstants)},
               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT
           )
           .bind_buffer(
               2, {.buffer = view.get_buffer(), .offset = 0, .range = sizeof(ViewDataGPU)},
               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT
           )
           .build();
    commands.bind_descriptor_set(1, sun_descriptor_set);

    commands.draw_triangle();

    commands.clear_descriptor_set(0);
    commands.clear_descriptor_set(1);

    commands.end_label();
}

void LightingPhase::add_raytraced_mesh_lighting(
    CommandBuffer& commands, VkDescriptorSet gbuffers_descriptor_set, BufferHandle view_buffer
) {
    auto& sun = scene->get_sun_light();
    auto& raytracing_scene = scene->get_raytracing_scene();
}

void LightingPhase::add_emissive_lighting(CommandBuffer& commands, const VkDescriptorSet gbuffer_descriptor_set) {
    commands.begin_label("Emissive Lighting");

    commands.bind_pipeline(emission_pipeline);

    commands.bind_descriptor_set(0, gbuffer_descriptor_set);

    commands.draw_triangle();

    commands.clear_descriptor_set(0);

    commands.end_label();
}
