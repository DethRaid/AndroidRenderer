#include "ui_phase.hpp"

#include "render/backend/command_buffer.hpp"
#include "render/scene_renderer.hpp"

UiPhase::UiPhase(SceneRenderer& renderer_in) :
        scene_renderer{renderer_in}, scene_color{scene_renderer.get_backend().get_white_texture_handle()} {
    create_upscale_pipeline();
}

void UiPhase::set_resources(TextureHandle scene_color_in) {
    scene_color = scene_color_in;
}

void UiPhase::render(CommandBuffer& commands, SceneView& view) {
    ZoneScoped;

    GpuZoneScoped(commands);

    commands.begin_label(__func__);

    upscale_scene_color(commands);

    // TODO: render UI

    commands.end_label();
}

void UiPhase::upscale_scene_color(CommandBuffer& commands) {
    auto& backend = scene_renderer.get_backend();

    const auto set = backend.create_frame_descriptor_builder()
                            .bind_image(0,
                                        {.sampler = backend.get_default_sampler(), .image = scene_color, .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                            .build();
    if (set) {
        commands.bind_descriptor_set(0, *set);
    }

    commands.bind_pipeline(upsample_pipeline);

    commands.draw_triangle();
}

void UiPhase::create_upscale_pipeline() {
    upsample_pipeline = scene_renderer.get_backend()
                                      .begin_building_pipeline("Scene Upscale")
                                      .set_vertex_shader("shaders/common/fullscreen.vert.spv")
                                      .set_fragment_shader("shaders/ui/scene_upsample.frag.spv")
                                      .set_depth_state(DepthStencilState{
                                              .enable_depth_test = false,
                                              .enable_depth_write = false,
                                      })
                                      .set_blend_state(0, {.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                                             VK_COLOR_COMPONENT_G_BIT |
                                                                             VK_COLOR_COMPONENT_B_BIT |
                                                                             VK_COLOR_COMPONENT_A_BIT})
                                      .build();

}
