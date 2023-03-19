#include "lpv_gv_voxelizer.hpp"

#include <glm/mat4x4.hpp>
#include <tl/optional.hpp>

#include "render/mesh_storage.hpp"
#include "render/render_scene.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/render_graph.hpp"
#include "shared/triangle.hpp"

LpvGvVoxelizer::~LpvGvVoxelizer() {
    auto& allocator = backend->get_global_allocator();
    deinit_resources(allocator);
}

void LpvGvVoxelizer::init_resources(RenderBackend& backend_in, const uint32_t voxel_texture_resolution) {
    resolution = voxel_texture_resolution;

    backend = &backend_in;
    auto& allocator = backend->get_global_allocator();
    deinit_resources(allocator);

    voxel_texture = allocator.create_volume_texture(
        "Voxels", VK_FORMAT_R16G16B16A16_SFLOAT, glm::uvec3{voxel_texture_resolution}, 1, TextureUsage::StorageImage
    );

    volume_uniform_buffer = allocator.create_buffer(
        "Voxel transform buffer", sizeof(glm::mat4), BufferUsage::UniformBuffer
    );

    transformed_primitive_cache = allocator.create_buffer(
        "Transformed triangles", sizeof(Triangle) * 16384, BufferUsage::StorageBuffer
    );

    {
        const auto bytes = *SystemInterface::get().load_file("shaders/voxelizer/clear.comp.spv");
        texture_clear_shader = *backend->create_compute_shader("Clear", bytes);
    }

    {
        const auto bytes = *SystemInterface::get().load_file("shaders/voxelizer/vertex_transformation.comp.spv");
        transform_verts_shader = *backend->create_compute_shader("Transform vertices", bytes);
    }

    {
        const auto bytes = *SystemInterface::get().load_file("shaders/voxelizer/binning.comp.spv");
        rasterize_primitives_shader = *backend->create_compute_shader("Rasterize to volume", bytes);
    }
    {
        const auto bytes = *SystemInterface::get().load_file("shaders/voxelizer/normalize_sh.comp.spv");
        normalize_gv_shader = *backend->create_compute_shader("Normalize SH", bytes);
    }
}

void LpvGvVoxelizer::deinit_resources(ResourceAllocator& allocator) {
    if (transformed_primitive_cache != BufferHandle::None) {
        allocator.destroy_buffer(transformed_primitive_cache);
        transformed_primitive_cache = BufferHandle::None;
    }
    if (voxel_texture != TextureHandle::None) {
        allocator.destroy_texture(voxel_texture);
        voxel_texture = TextureHandle::None;
    }
    if (volume_uniform_buffer != BufferHandle::None) {
        allocator.destroy_buffer(volume_uniform_buffer);
        volume_uniform_buffer = BufferHandle::None;
    }
}

void LpvGvVoxelizer::set_scene(RenderScene& scene_in, MeshStorage& meshes_in) {
    scene = &scene_in;
    meshes = &meshes_in;
}

void LpvGvVoxelizer::voxelize_scene(
    RenderGraph& graph, const glm::vec3& voxel_bounds_min, const glm::vec3& voxel_bounds_max
) const {
    const auto scale = voxel_bounds_max - voxel_bounds_min;
    const auto center = voxel_bounds_min + (scale * 0.5f);
    
    const auto bias_mat = glm::mat4{
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.5f, 0.0f,
        0.5f, 0.5f, 0.5f, 1.0f
    };

    auto world_to_voxel = glm::mat4{1.f};
    world_to_voxel = glm::scale(world_to_voxel, glm::vec3{1.f} / scale);
    world_to_voxel = glm::translate(world_to_voxel, -center);
    world_to_voxel = bias_mat * world_to_voxel;

    graph.add_compute_pass(
        {
            .name = "Clear voxels",
            .textures = {
                {
                    voxel_texture,
                    {
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL
                    }                    
                }
            },
            .buffers = {
                {
                    volume_uniform_buffer,
                    {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT}
                }
            },
            .execute = [&, world_to_voxel](CommandBuffer& commands) {
                const auto set = *backend->create_frame_descriptor_builder()
                                         .bind_image(
                                             0, {.image = voxel_texture, .image_layout = VK_IMAGE_LAYOUT_GENERAL},
                                             VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT
                                         )
                                         .build();

                commands.bind_descriptor_set(0, set);

                commands.bind_shader(texture_clear_shader);

                commands.dispatch(resolution / 4, resolution / 4, resolution / 4);

                commands.clear_descriptor_set(0);

                commands.update_buffer(volume_uniform_buffer, world_to_voxel);
            }
        }
    );

    const auto& primitives = scene->get_solid_primitives();
    uint32_t primitive_index = 0;
    for (const auto& primitive : primitives) {
        graph.add_compute_pass(
            ComputePass{
                .name = "Transform primitive",
                .buffers = {
                    {
                        meshes->get_vertex_position_buffer(),
                        {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_SHADER_READ_BIT_KHR}
                    },
                    {
                        meshes->get_vertex_data_buffer(),
                        {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_SHADER_READ_BIT_KHR}
                    },
                    {
                        meshes->get_index_buffer(),
                        {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_SHADER_READ_BIT_KHR}
                    },
                    {
                        volume_uniform_buffer,
                        {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_UNIFORM_READ_BIT_KHR}
                    },
                    {
                        scene->get_primitive_buffer(),
                        {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_SHADER_READ_BIT_KHR}
                    },
                    {
                        transformed_primitive_cache,
                        {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_SHADER_WRITE_BIT_KHR}
                    },
                },
                .execute = [&](CommandBuffer& commands) {
                    GpuZoneScopedN(commands, "Transform primitive")
                    const auto set = *backend->create_frame_descriptor_builder()
                                             .bind_buffer(
                                                 0, {.buffer = meshes->get_vertex_position_buffer()},
                                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .bind_buffer(
                                                 1, {.buffer = meshes->get_vertex_data_buffer()},
                                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .bind_buffer(
                                                 2, {.buffer = meshes->get_index_buffer()},
                                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .bind_buffer(
                                                 3, {.buffer = volume_uniform_buffer},
                                                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .bind_buffer(
                                                 4, {.buffer = scene->get_primitive_buffer()},
                                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .bind_buffer(
                                                 5, {.buffer = transformed_primitive_cache},
                                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .build();

                    commands.bind_descriptor_set(0, set);

                    commands.bind_shader(transform_verts_shader);

                    commands.set_push_constant(0, primitive_index);
                    commands.set_push_constant(1, static_cast<uint32_t>(primitive->mesh.first_vertex));
                    commands.set_push_constant(2, static_cast<uint32_t>(primitive->mesh.first_index));
                    commands.set_push_constant(3, primitive->mesh.num_indices);

                    commands.dispatch(primitive->mesh.num_indices / 96 + 1, 1, 1);

                    commands.clear_descriptor_set(0);
                }
            }
        );

        graph.add_compute_pass(
            {
                .name = "Rasterize_primitive",
                .textures = {
                    {
                        voxel_texture,
                        {
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_SHADER_WRITE_BIT_KHR,
                            VK_IMAGE_LAYOUT_GENERAL
                        }
                    }
                },
                .buffers = {
                    {
                        transformed_primitive_cache,
                        {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_SHADER_READ_BIT_KHR}
                    }
                },
                .execute = [&](CommandBuffer& commands) {
                    GpuZoneScopedN(commands, "Rasterize primitive")
                    const auto set = *backend->create_frame_descriptor_builder()
                                             .bind_buffer(
                                                 0, {.buffer = transformed_primitive_cache},
                                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .bind_image(
                                                 1, {.image = voxel_texture, .image_layout = VK_IMAGE_LAYOUT_GENERAL},
                                                 VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .build();

                    commands.bind_descriptor_set(0, set);

                    commands.set_push_constant(0, primitive->mesh.num_indices);

                    commands.bind_shader(rasterize_primitives_shader);

                    commands.dispatch(8, 8, 8);

                    commands.clear_descriptor_set(0);
                }
            }
        );

        primitive_index++;
    }
}

TextureHandle LpvGvVoxelizer::get_texture() const {
    return voxel_texture;
}
