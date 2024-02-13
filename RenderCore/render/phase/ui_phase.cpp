#include "ui_phase.hpp"

#include "render/backend/command_buffer.hpp"
#include "render/scene_renderer.hpp"

UiPhase::UiPhase(SceneRenderer& renderer_in) :
    scene_renderer{renderer_in}, scene_color{scene_renderer.get_backend().get_white_texture_handle()} {
    create_upscale_pipeline();

    bilinear_sampler = renderer_in.get_backend().get_global_allocator().get_sampler(
        {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .maxLod = 16,
        }
    );
}

void UiPhase::set_resources(TextureHandle scene_color_in) {
    scene_color = scene_color_in;
}

void UiPhase::render(CommandBuffer& commands, const SceneTransform& view, const TextureHandle bloom_texture) {
    GpuZoneScopedN(commands, "UiPhase::render");

    commands.begin_label(__func__);

    upscale_scene_color(commands, bloom_texture);

    // TODO: render in-game UI

    render_imgui_items(commands);

    commands.end_label();
}

void UiPhase::set_imgui_draw_data(ImDrawData* im_draw_data) {
    imgui_draw_data = im_draw_data;
}

void UiPhase::upscale_scene_color(CommandBuffer& commands, const TextureHandle bloom_texture) {
    auto& backend = scene_renderer.get_backend();

    const auto set = backend.create_frame_descriptor_builder()
                            .bind_image(
                                0,
                                {
                                    .sampler = bilinear_sampler, .image = scene_color,
                                    .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                },
                                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT
                            )
                            .bind_image(
                                1, {
                                    .sampler = bilinear_sampler, .image = bloom_texture,
                                    .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                }, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT
                            )
                            .build();

    commands.bind_descriptor_set(0, *set);
    
    commands.bind_pipeline(upsample_pipeline);

    commands.draw_triangle();

    commands.clear_descriptor_set(0);
}

void UiPhase::render_imgui_items(CommandBuffer& commands) {
    
}

void UiPhase::create_upscale_pipeline() {
    upsample_pipeline = scene_renderer.get_backend()
                                      .begin_building_pipeline("Scene Upscale")
                                      .set_vertex_shader("shaders/common/fullscreen.vert.spv")
                                      .set_fragment_shader("shaders/ui/scene_upsample.frag.spv")
                                      .set_depth_state(
                                          DepthStencilState{
                                              .enable_depth_test = false,
                                              .enable_depth_write = false,
                                          }
                                      )
                                      .set_blend_state(
                                          0, {
                                              .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                              VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT |
                                              VK_COLOR_COMPONENT_A_BIT
                                          }
                                      )
                                      .build();
}
