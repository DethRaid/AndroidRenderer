#include "lpv_gv_voxelizer.hpp"

#include <glm/mat4x4.hpp>
#include <tl/optional.hpp>

#include "render/mesh_storage.hpp"
#include "render/render_scene.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/render_graph.hpp"
#include "shared/triangle.hpp"

/**
 * Max number of triangles to process before dispatching binning and rasterization
 *
 * Some quick and dirty tests
 *
 * 292144: 100 ms / frame
 * 65536: 30 ms / frame
 * 32768: 26 ms / frame
 * 16384: 25 ms / frame
 * 8192: 20 ms / frame
 * 4096: 13 ms / frame
 * 2048: 9 ms / frame
 *
 * Note that some scene primitives have 16k triangles or more.. these smaller numbers are not good
 * Sponza has 23208 triangles in one primitive :|
 */
constexpr const uint32_t max_batched_triangles = 32768;

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

    transformed_primitive_cache_a = allocator.create_buffer(
        "Transformed triangles A", sizeof(Triangle) * max_batched_triangles, BufferUsage::StorageBuffer
    );

    triangle_sh_cache_a = allocator.create_buffer(
        "Triangles SH A", sizeof(glm::vec4) * max_batched_triangles, BufferUsage::StorageBuffer
    );

    transformed_primitive_cache_b = allocator.create_buffer(
        "Transformed triangles B", sizeof(Triangle) * max_batched_triangles, BufferUsage::StorageBuffer
    );

    triangle_sh_cache_b = allocator.create_buffer(
        "Triangles SH A", sizeof(glm::vec4) * max_batched_triangles, BufferUsage::StorageBuffer
    );

    bins_a = allocator.create_buffer(
        "Bin bitmask A", max_batched_triangles / 32 * 8 * 8 * 8, BufferUsage::StorageBuffer
    );

    bins_b = allocator.create_buffer(
        "Bin bitmask B", max_batched_triangles / 32 * 8 * 8 * 8, BufferUsage::StorageBuffer
    );

    cell_bitmask_coarse = allocator.create_buffer(
        "Coarse cell bitmask", max_batched_triangles * 32, BufferUsage::StorageBuffer
    );

    cell_bitmask = allocator.create_buffer(
        "Cell bitmask", max_batched_triangles / 32 * 32 * 32 * 32, BufferUsage::StorageBuffer
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
        const auto bytes = *SystemInterface::get().load_file("shaders/voxelizer/binning_coarse.comp.spv");
        coarse_binning_shader = *backend->create_compute_shader("Bin triangles to bins", bytes);
    }
    {
        const auto bytes = *SystemInterface::get().load_file("shaders/voxelizer/binning_fine.comp.spv");
        fine_binning_shader = *backend->create_compute_shader("Bin triangles to calls", bytes);
    }
    {
        const auto bytes = *SystemInterface::get().load_file("shaders/voxelizer/rasterization.comp.spv");
        rasterize_primitives_shader = *backend->create_compute_shader("Rasterize to volume", bytes);
    }
    {
        const auto bytes = *SystemInterface::get().load_file("shaders/voxelizer/normalize_sh.comp.spv");
        normalize_gv_shader = *backend->create_compute_shader("Normalize SH", bytes);
    }

    const auto create_info = VkEventCreateInfo{
        .sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
        .flags = VK_EVENT_CREATE_DEVICE_ONLY_BIT,
    };
    vkCreateEvent(backend->get_device().device, &create_info, nullptr, &top_half_event);
    vkCreateEvent(backend->get_device().device, &create_info, nullptr, &bottom_half_event);
}

void LpvGvVoxelizer::deinit_resources(ResourceAllocator& allocator) {
    if (transformed_primitive_cache_a != BufferHandle::None) {
        allocator.destroy_buffer(transformed_primitive_cache_a);
        transformed_primitive_cache_a = BufferHandle::None;
    }
    if (transformed_primitive_cache_b != BufferHandle::None) {
        allocator.destroy_buffer(transformed_primitive_cache_b);
        transformed_primitive_cache_b = BufferHandle::None;
    }
    if (bins_a != BufferHandle::None) {
        allocator.destroy_buffer(bins_a);
        bins_a = BufferHandle::None;
    }
    if (bins_b != BufferHandle::None) {
        allocator.destroy_buffer(bins_b);
        bins_b = BufferHandle::None;
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

    auto transformed_triangles_write = transformed_primitive_cache_a;
    auto transformed_triangles_read = transformed_primitive_cache_b;
    auto triangle_sh_write = triangle_sh_cache_a;
    auto triangle_sh_read = triangle_sh_cache_b;

    auto bins_write = bins_a;
    auto bins_read = bins_b;

    const auto& primitives = scene->get_solid_primitives();
    auto primitive_index = 0u;

    // Transform vertices until we fill up the triangle cache, then dispatch the binning and rasterization shaders
    while (primitive_index < primitives.size()) {
        auto triangle_cache_offset = 0u;
        auto last_primitive_size = 0u;

        const auto triangle_shader_set = *backend->create_frame_descriptor_builder()
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
                                                     5, {.buffer = transformed_triangles_write},
                                                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
                                                 )

                                                 .bind_buffer(
                                                     6, {.buffer = triangle_sh_write},
                                                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
                                                 )
                                                 .build();


        // TODO: The barrier for the transformed primitive cache should only synchronize the range this pass writes
        // to - not the whole buffer.
        // TODO: Add the ability to shade a subset of the triangles in a primitive
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
                        transformed_triangles_write,
                        {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_SHADER_WRITE_BIT_KHR}
                    },
                    {
                        triangle_sh_write,
                        {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_SHADER_WRITE_BIT_KHR}
                    },
                },
                .execute = [&, this](CommandBuffer& commands) {
                    GpuZoneScopedN(commands, "Transform primitives")

                    commands.bind_descriptor_set(0, triangle_shader_set);

                    commands.bind_shader(transform_verts_shader);

                    while (triangle_cache_offset + last_primitive_size < max_batched_triangles && primitive_index <
                        primitives.size()) {
                        triangle_cache_offset += last_primitive_size;

                        auto& primitive = primitives[primitive_index];

                        commands.set_push_constant(0, primitive_index);
                        commands.set_push_constant(1, static_cast<uint32_t>(primitive->mesh.first_vertex));
                        commands.set_push_constant(2, static_cast<uint32_t>(primitive->mesh.first_index));
                        commands.set_push_constant(3, primitive->mesh.num_indices);
                        commands.set_push_constant(4, triangle_cache_offset);

                        // / 3 because we have one thread per triangle, / 96 because we have 96 threads per workgroup
                        commands.dispatch(primitive->mesh.num_indices / 96 + 1, 1, 1);

                        last_primitive_size = primitive->mesh.num_indices / 3;
                        primitive_index++;
                    }

                    commands.clear_descriptor_set(0);
                }
            }
        );

        std::swap(transformed_triangles_read, transformed_triangles_write);
        std::swap(triangle_sh_read, triangle_sh_write);

        // Now that the triangle buffer is full, bin and rasterize the triangles
        graph.add_compute_pass(
            {
                .name = "Bin triangles Low Res",
                .buffers = {
                    {
                        transformed_triangles_read,
                        {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_SHADER_READ_BIT_KHR}
                    },
                    {
                        bins_write,
                        {
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
                            VK_ACCESS_2_SHADER_READ_BIT_KHR | VK_ACCESS_2_SHADER_WRITE_BIT_KHR
                        }
                    }
                },
                .execute = [&, triangle_cache_offset = triangle_cache_offset](CommandBuffer& commands) {
                    GpuZoneScopedN(commands, "Bin Triangles Low Res")

                    const auto set = *backend->create_frame_descriptor_builder()
                                             .bind_buffer(
                                                 0, {.buffer = transformed_triangles_read},
                                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                 VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .bind_buffer(
                                                 1, {
                                                     .buffer = bins_write
                                                 },
                                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                 VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .build();

                    commands.bind_descriptor_set(0, set);

                    commands.bind_shader(coarse_binning_shader);

                    commands.set_push_constant(0, triangle_cache_offset);

                    commands.dispatch(8, 8, 8);

                    commands.clear_descriptor_set(0);
                }
            }
        );

        std::swap(bins_write, bins_read);

        graph.add_compute_pass(
            {
                .name = "Bin Triangles High Res",
                .buffers = {
                    {
                        transformed_triangles_read,
                        {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_SHADER_READ_BIT_KHR}
                    },
                    {
                        bins_read,
                        {
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
                            VK_ACCESS_2_SHADER_READ_BIT_KHR
                        }
                    },
                    {
                        cell_bitmask_coarse,
                        {
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
                            VK_ACCESS_2_SHADER_READ_BIT_KHR | VK_ACCESS_2_SHADER_WRITE_BIT_KHR
                        }
                    },
                    {
                        cell_bitmask,
                        {
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
                            VK_ACCESS_2_SHADER_READ_BIT_KHR | VK_ACCESS_2_SHADER_WRITE_BIT_KHR
                        }
                    }
                },
                .execute = [&](CommandBuffer& commands) {
                    GpuZoneScopedN(commands, "Bin Triangles High Res")

                    const auto set = *backend->create_frame_descriptor_builder()
                                             .bind_buffer(
                                                 0, {.buffer = transformed_triangles_read},
                                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                 VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .bind_buffer(
                                                 1, {.buffer = bins_read},
                                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                 VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .bind_buffer(
                                                 2, {.buffer = cell_bitmask_coarse},
                                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                 VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .bind_buffer(
                                                 3, {.buffer = cell_bitmask},
                                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                 VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .build();

                    commands.bind_descriptor_set(0, set);
                    commands.bind_shader(fine_binning_shader);

                    commands.set_push_constant(0, triangle_cache_offset);

                    // Workgroups are 96 threads wide
                    commands.dispatch(32 * 11, 32, 32);

                    commands.clear_descriptor_set(0);
                }
            }

        );

        graph.add_compute_pass(
            {
                .name = "Rasterize triangles",
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
                        triangle_sh_read,
                        {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_SHADER_READ_BIT_KHR}
                    },
                    {
                        cell_bitmask_coarse,
                        {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_SHADER_READ_BIT_KHR}
                    },
                    {
                        cell_bitmask,
                        {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_SHADER_READ_BIT_KHR}
                    }
                },
                .execute = [&](CommandBuffer& commands) {
                    GpuZoneScopedN(commands, "Rasterize triangles")
                    const auto set = *backend->create_frame_descriptor_builder()
                                             .bind_buffer(
                                                 0, {.buffer = triangle_sh_read},
                                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .bind_buffer(
                                                 1, {.buffer = cell_bitmask_coarse},
                                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .bind_buffer(
                                                 2, {.buffer = cell_bitmask},
                                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .bind_image(
                                                 3, {
                                                     .image = voxel_texture, .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                                 },
                                                 VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .build();

                    commands.bind_descriptor_set(0, set);

                    commands.bind_shader(rasterize_primitives_shader);

                    // TODO: Each thread should read one mask in the coarse bitmask - aka 32 masks in the fine bitmask
                    // Might also split up the work using events... same as above? Merge these passes?

                    commands.dispatch(8, 8, 8);

                    commands.clear_descriptor_set(0);
                }
            }
        );
    }
}

TextureHandle LpvGvVoxelizer::get_texture() const {
    return voxel_texture;
}
