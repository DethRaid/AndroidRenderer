#include "gbuffer_phase.hpp"

#include "render/indirect_drawing_utils.hpp"
#include "render/material_pipelines.hpp"
#include "render/material_storage.hpp"
#include "render/render_scene.hpp"
#include "render/scene_view.hpp"
#include "render/backend/render_backend.hpp"

GbufferPhase::GbufferPhase() {}

void GbufferPhase::render(
    RenderGraph& graph, const RenderScene& scene, const IndirectDrawingBuffers& buffers,
    const IndirectDrawingBuffers& visible_masked_buffers, const TextureHandle gbuffer_depth,
    const TextureHandle gbuffer_color, const TextureHandle gbuffer_normals, const TextureHandle gbuffer_data,
    const TextureHandle gbuffer_emission, const std::optional<TextureHandle> shading_rate, const SceneView& player_view
) {
    const auto& pipelines = scene.get_material_storage().get_pipelines();
    const auto solid_pso = pipelines.get_gbuffer_pso();

    auto& backend = RenderBackend::get();
    auto gbuffer_set = backend.get_transient_descriptor_allocator().build_set(solid_pso, 0)
                              .bind(player_view.get_buffer())
                              .bind(scene.get_primitive_buffer())
                              .bind(scene.get_material_storage().get_material_instance_buffer())
                              .build();

    const auto masked_pso = pipelines.get_gbuffer_masked_pso();
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
                {
                    visible_masked_buffers.commands,
                    VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                    VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT
                },
                {
                    visible_masked_buffers.count,
                    VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                    VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT
                },
                {
                    visible_masked_buffers.primitive_ids,
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

                scene.draw_opaque(commands, buffers, solid_pso);

                scene.draw_masked(commands, visible_masked_buffers, masked_pso);

                commands.clear_descriptor_set(0);
            }
        });
}
