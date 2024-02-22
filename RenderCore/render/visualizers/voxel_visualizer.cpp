#include "voxel_visualizer.hpp"

#include "render/render_scene.hpp"
#include "render/backend/render_backend.hpp"

VoxelVisualizer::VoxelVisualizer(RenderBackend& backend_in) {
    visualization_pipeline =
        backend_in.begin_building_pipeline("Voxel Visualizer")
                  .set_vertex_shader("voxelizer/visualizer.vert.spv")
                  .set_fragment_shader("voxelizer/visualizer.frag.spv")
                  .set_depth_state({.enable_depth_test = false, .enable_depth_write = false})
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
        0, 1, 4,
        4, 1, 5,
        // Top
        2, 6, 3,
        3, 6, 7,
        // Front
        6, 4, 7,
        7, 4, 5,
        // Right
        7, 5, 3,
        3, 5, 1,
        // Back
        3, 1, 2,
        2, 1, 0,
        // Left
        4, 6, 0,
        0, 6, 2
    };

    cube_vertex_buffer = allocator.create_buffer(
        "Cube vertex buffer", 8 * sizeof(glm::vec3), BufferUsage::VertexBuffer
    );
    upload_queue.upload_to_buffer(cube_vertex_buffer, std::span{vertex_data.data(), vertex_data.size()});

    cube_index_buffer = allocator.create_buffer("Cube index buffer", 36 * sizeof(uint16_t), BufferUsage::IndexBuffer);
    upload_queue.upload_to_buffer(cube_index_buffer, std::span{index_data.data(), index_data.size()});
}

void VoxelVisualizer::render(RenderGraph& render_graph, RenderScene& scene, TextureHandle output_image) {
    render_graph.add_render_pass(
        {
            .name = "Voxel Visualization",
            .textures = {},
            .buffers = {},
            .attachments = {output_image},
            .subpasses = {
                {
                    .name = "Subpass",
                    .color_attachments = {0},
                    .execute = [=, this](CommandBuffer& commands) {
                        const auto primitives_buffer = scene.get_primitive_buffer();

                        commands.bind_pipeline(visualization_pipeline);

                        commands.bind_index_buffer<uint16_t>(cube_index_buffer);
                        commands.bind_vertex_buffer(0, cube_vertex_buffer);

                        commands.draw_indexed(36, scene.get_total_num_primitives(), 0, 0, 0);
                    }
                }
            }
        }
    );
}
