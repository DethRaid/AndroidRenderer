#include "mesh_voxelizer.hpp"

#include "glm/ext/matrix_clip_space.hpp"
#include "render/mesh_storage.hpp"
#include "render/backend/render_backend.hpp"
#include "shared/view_data.hpp"

MeshVoxelizer::MeshVoxelizer(RenderBackend& backend_in) : backend{&backend_in} {
    voxelization_pipeline = backend->begin_building_pipeline("Voxelizer")
                                   .set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                                   .set_vertex_shader("shaders/voxelizer/voxelizer.vert.spv")
                                   .set_geometry_shader("shaders/voxelizer/voxelizer.geom.spv")
                                   .set_fragment_shader("shaders/voxelizer/voxelizer.frag.spv")
                                   .set_depth_state(
                                       {
                                           .enable_depth_test = false, .enable_depth_write = false,
                                           .compare_op = VK_COMPARE_OP_ALWAYS
                                       }
                                   )
                                   .set_raster_state({.cull_mode = VK_CULL_MODE_NONE})
                                   .build();
}

TextureHandle MeshVoxelizer::voxelize_primitive(
    RenderGraph& graph, const MeshPrimitiveHandle primitive, const MeshStorage& mesh_storage,
    const BufferHandle primitive_buffer, const float voxel_size, const Mode mode
) {
    // Create a 3D texture big enough to hold the mesh's bounding sphere. There will be some wasted space, maybe we can copy to a smaller texture at some point?
    const auto bounds = primitive->mesh->bounds;
    const auto voxel_texture_resolution = glm::uvec3{bounds * 2.f * voxel_size};

    auto& allocator = backend->get_global_allocator();

    const auto voxels = allocator.create_volume_texture(
        "Mesh voxel buffer", VK_FORMAT_R8G8B8A8_UNORM, voxel_texture_resolution, 1, TextureUsage::StorageImage
    );

    const auto frustums_buffer = allocator.create_buffer(
        "Voxelizer frustums", sizeof(glm::mat4), BufferUsage::StagingBuffer
    );
    auto* bounds_frustum_matrix = allocator.map_buffer<glm::mat4>(frustums_buffer);
    *bounds_frustum_matrix = glm::ortho(-bounds.x, bounds.x, bounds.y, -bounds.y, bounds.z, -bounds.z);

    graph.begin_render_pass(
        {
            .name = "Vozelization",
            .textures = {
                {
                    voxels,
                    {
                        .stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                        .access = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                        .layout = VK_IMAGE_LAYOUT_GENERAL
                    }
                }
            },
            .buffers = {
                {
                    primitive_buffer,
                    {.stage = VK_SHADER_STAGE_FRAGMENT_BIT, .access = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT}
                },
                {frustums_buffer, {.stage = VK_SHADER_STAGE_VERTEX_BIT, .access = VK_ACCESS_2_UNIFORM_READ_BIT}}
            }
        }
    );

    graph.add_subpass(
        {
            .name = "Voxelization", .execute = [=, this, &mesh_storage](CommandBuffer& commands) {
                const auto set = backend->create_frame_descriptor_builder()
                                        .bind_image(
                                            0, {.image = voxels, .image_layout = VK_IMAGE_LAYOUT_GENERAL},
                                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                            VK_SHADER_STAGE_FRAGMENT_BIT
                                        )
                                        .bind_buffer(
                                            1, {.buffer = primitive_buffer}, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
                                        )
                                        .bind_buffer(
                                            2, {.buffer = frustums_buffer}, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                            VK_SHADER_STAGE_VERTEX_BIT
                                        )
                                        .build();

                commands.bind_vertex_buffer(0, mesh_storage.get_vertex_position_buffer());
                commands.bind_vertex_buffer(1, mesh_storage.get_vertex_data_buffer());
                commands.bind_index_buffer(mesh_storage.get_index_buffer());

                commands.bind_descriptor_set(0, *set);
                commands.bind_descriptor_set(1, backend->get_texture_descriptor_pool().get_descriptor_set());

                commands.set_push_constant(0, primitive.index);

                commands.bind_pipeline(voxelization_pipeline);

                const auto& mesh = primitive->mesh;
                commands.draw_indexed(
                    mesh->num_indices, 1, static_cast<uint32_t>(mesh->first_index),
                    static_cast<uint32_t>(mesh->first_vertex), 0
                );
            }
        }
    );

    graph.end_render_pass();

    return voxels;
}
