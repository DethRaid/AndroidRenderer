#include "motion_vectors_phase.hpp"

#include "render/indirect_drawing_utils.hpp"
#include "render/mesh_drawer.hpp"
#include "render/render_scene.hpp"
#include "render/backend/render_backend.hpp"

MotionVectorsPhase::MotionVectorsPhase() {
    motion_vectors_pso = RenderBackend::get()
                         .begin_building_pipeline("motion_vectors_pso")
                         .use_standard_vertex_layout()
                         .set_vertex_shader("shaders/motion_vectors/motion_vectors.vert.spv")
                         .set_fragment_shader("shaders/motion_vectors/motion_vectors_opaque.frag.spv")
                         .set_depth_state(
                             {
                                 .enable_depth_test = true,
                                 .enable_depth_write = false,
                                 .compare_op = VK_COMPARE_OP_EQUAL
                             })
                         .build();
}

void MotionVectorsPhase::set_render_resolution(const glm::uvec2& resolution) {
    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();

    if(motion_vectors) {
        if(motion_vectors->create_info.extent.width != resolution.x || motion_vectors->create_info.extent.height !=
            resolution.y) {
            allocator.destroy_texture(motion_vectors);
            motion_vectors = nullptr;
        }
    }

    if(motion_vectors == nullptr) {
        motion_vectors = allocator.create_texture(
            "motion_vectors",
            {.format = VK_FORMAT_R16G16_SFLOAT, .resolution = resolution, .usage = TextureUsage::RenderTarget}
        );
    }
}

void MotionVectorsPhase::render(
    RenderGraph& graph, const RenderScene& scene, const BufferHandle view_data_buffer,
    const TextureHandle depth_buffer, const IndirectDrawingBuffers& buffers, const IndirectDrawingBuffers& masked_buffers
) {
    auto& allocator = RenderBackend::get().get_transient_descriptor_allocator();
    const auto set = allocator.build_set(motion_vectors_pso, 0)
                              .bind(view_data_buffer)
                              .bind(scene.get_primitive_buffer())
                              .build();

    graph.add_render_pass(
        {
            .name = "motion_vectors",
            .buffers = {
                {
                    .buffer = buffers.commands,
                    .stage = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                    .access = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT
                },
                {
                    .buffer = buffers.count,
                    .stage = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                    .access = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT
                },
                {
                    .buffer = buffers.primitive_ids,
                    .stage = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
                    .access = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT
                }
            },
            .descriptor_sets = {set},
            .color_attachments = {
                {
                    .image = motion_vectors,
                    .load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .clear_value = {.color = {.float32 = {0, 0, 0, 0}}}
                }
            },
            .depth_attachment = RenderingAttachmentInfo{.image = depth_buffer},
            .execute = [&](CommandBuffer& commands) {
                commands.bind_descriptor_set(0, set);

                scene.draw_opaque(commands, buffers, motion_vectors_pso);

                commands.clear_descriptor_set(0);
            }
        });
}

TextureHandle MotionVectorsPhase::get_motion_vectors() const {
    return motion_vectors;
}
