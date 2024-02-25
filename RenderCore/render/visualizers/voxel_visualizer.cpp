#include "voxel_visualizer.hpp"

#include "render/render_scene.hpp"
#include "render/backend/render_backend.hpp"

VoxelVisualizer::VoxelVisualizer(RenderBackend& backend_in) : backend{backend_in} {
    visualization_pipeline =
        backend_in.begin_building_pipeline("Voxel Visualizer")
                  .set_vertex_shader("shaders/voxelizer/visualizer.vert.spv")
                  .set_fragment_shader("shaders/voxelizer/visualizer.frag.spv")
                  .build();

    auto& allocator = backend_in.get_global_allocator();
    auto& upload_queue = backend_in.get_upload_queue();

    constexpr auto vertex_data = std::array<glm::vec3, 8>{
        /* 0 */ glm::vec3{-1, -1, -1},
        /* 1 */ {1, -1, -1},
        /* 2 */ {-1, 1, -1},
        /* 3 */ {1, 1, -1},
        /* 4 */ {-1, -1, 1},
        /* 5 */ {1, -1, 1},
        /* 6 */ {-1, 1, 1},
        /* 7 */ {1, 1, 1}
    };
    constexpr auto index_data = std::array<uint16_t, 36>{
        // Bottom
        0, 4, 1,
        4, 5, 1,
        // Top
        2, 3, 6,
        3, 7, 6,
        // Front
        6, 7, 4,
        7, 5, 4,
        // Right
        7, 3, 5,
        3, 1, 5,
        // Back
        3, 2, 1,
        2, 0, 1,
        // Left
        4, 0, 6,
        0, 2, 6,
    };

    cube_vertex_buffer = allocator.create_buffer(
        "Cube vertex buffer", 8 * sizeof(glm::vec3), BufferUsage::VertexBuffer
    );
    upload_queue.upload_to_buffer(cube_vertex_buffer, std::span{vertex_data.data(), vertex_data.size()});

    cube_index_buffer = allocator.create_buffer("Cube index buffer", 36 * sizeof(uint16_t), BufferUsage::IndexBuffer);
    upload_queue.upload_to_buffer(cube_index_buffer, std::span{index_data.data(), index_data.size()});
}

void VoxelVisualizer::render(
    RenderGraph& render_graph, const RenderScene& scene, TextureHandle output_image, BufferHandle view_uniform_buffer
) {
    // Draw one cube for each primitive in the scene. Draw their front faces. The vertex shader scales the box to match
    // the primitive's bounding box and calculates the worldspace view vector. The fragment shader raymarches along the
    // view vector, sampling the voxel texture at each step. If the ray hits a solid voxel, the fragment shader samples
    // the voxel and returns. If the ray hits the depth buffer, or reaches the outside of the voxel texture, the
    // fragment shader does nothing and returns
    //
    // This means objects will disappear when you're inside their bounding boxes. This isn't ideal but it makes the
    // visualizer simpler. The other option is to only draw the primitives that were visible this frame, without depth
    // testing. Draw their back faces, then send a ray towards the front face, then raymarch from the hit position (or
    // the near plane) away from the camera. Disable, but more complex

    const auto descriptor_set = backend.begin_building_descriptor_set(visualization_pipeline, 1)
                                       .bind(0, view_uniform_buffer)
                                       .bind(1, scene.get_primitive_buffer())
                                       .finalize();

    render_graph.begin_render_pass(
        {
            .name = "Voxel Visualization",
            .buffers = {
                {
                    cube_index_buffer,
                    {
                        .stage = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
                        .access = VK_ACCESS_2_INDEX_READ_BIT
                    }
                },
                {
                    cube_vertex_buffer,
                    {
                        .stage = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
                        .access = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT
                    }
                },
            },
            .descriptor_sets = {descriptor_set},
            .attachments = {output_image}
        }
    );
    render_graph.add_subpass(
        {
            .name = "Subpass",
            .color_attachments = {0},
            .execute = [=, this, &scene](CommandBuffer& commands) {
                commands.bind_pipeline(visualization_pipeline);

                commands.bind_descriptor_set(1, descriptor_set.get_vk_descriptor_set());

                commands.bind_index_buffer<uint16_t>(cube_index_buffer);
                commands.bind_vertex_buffer(0, cube_vertex_buffer);

                commands.draw_indexed(36, scene.get_total_num_primitives(), 0, 0, 0);
            }
        }
    );
    render_graph.end_render_pass();
}
