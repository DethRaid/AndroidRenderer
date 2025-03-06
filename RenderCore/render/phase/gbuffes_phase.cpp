#include "gbuffes_phase.hpp"

#include "render/indirect_drawing_utils.hpp"
#include "render/render_scene.hpp"
#include "render/scene_view.hpp"
#include "render/backend/render_backend.hpp"

GbuffersPhase::GbuffersPhase() {
    auto& backend = RenderBackend::get();
    const auto blend_state = VkPipelineColorBlendAttachmentState{
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT
    };
    opaque_pso = backend.begin_building_pipeline("gbuffer_opaque")
                        .set_vertex_shader("shaders/deferred/basic.vert.spv")
                        .set_fragment_shader("shaders/deferred/standard_pbr.frag.spv")
                        .set_depth_state(
                            {
                                .enable_depth_test = true,
                                .enable_depth_write = false,
                                .compare_op = VK_COMPARE_OP_EQUAL
                            }
                        )
                        .set_raster_state(
                            {
                                .front_face = VK_FRONT_FACE_CLOCKWISE
                            }
                        )
                        .set_blend_state(0, blend_state)
                        .set_blend_state(1, blend_state)
                        .set_blend_state(2, blend_state)
                        .set_blend_state(3, blend_state)
                        .build();

}

void GbuffersPhase::render(
    RenderGraph& graph, const SceneDrawer& drawer, const IndirectDrawingBuffers& buffers,
    const TextureHandle gbuffer_depth, const TextureHandle gbuffer_color, const TextureHandle gbuffer_normals,
    const TextureHandle gbuffer_data, const TextureHandle gbuffer_emission,
    const std::optional<TextureHandle> shading_rate, const SceneView& player_view
) {
    auto& backend = RenderBackend::get();
    auto gbuffer_set = backend.get_transient_descriptor_allocator().build_set(opaque_pso, 0)
                              .bind(player_view.get_buffer())
                              .bind(drawer.get_scene().get_primitive_buffer())
                              .build();
    graph.add_render_pass(
        DynamicRenderingPass{
            .name = "gbuffer",
            .buffers = {
                {
                    buffers.commands,
                    VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                    VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT
                },
                {
                    buffers.count,
                    VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                    VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT
                },
                {
                    buffers.primitive_ids,
                    VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                    VK_ACCESS_2_SHADER_READ_BIT
                },
            },
            .descriptor_sets = {gbuffer_set},
            .color_attachments = {
                RenderingAttachmentInfo{
                    .image = gbuffer_color,
                    .load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .store_op = VK_ATTACHMENT_STORE_OP_STORE
                },
                RenderingAttachmentInfo{
                    .image = gbuffer_normals,
                    .load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .store_op = VK_ATTACHMENT_STORE_OP_STORE,
                    .clear_value = {.color = {.float32 = {0.5f, 0.5f, 1.f, 0}}}
                },
                RenderingAttachmentInfo{
                    .image = gbuffer_data,
                    .load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .store_op = VK_ATTACHMENT_STORE_OP_STORE
                },
                RenderingAttachmentInfo{
                    .image = gbuffer_emission,
                    .load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .store_op = VK_ATTACHMENT_STORE_OP_STORE
                },
            },
            .depth_attachment = RenderingAttachmentInfo{.image = gbuffer_depth},
            .shading_rate_image = shading_rate,
            .execute = [&](CommandBuffer& commands) {
                commands.bind_descriptor_set(0, gbuffer_set);

                drawer.draw_indirect(commands, opaque_pso, buffers);

                commands.clear_descriptor_set(0);
            }
        });
}
