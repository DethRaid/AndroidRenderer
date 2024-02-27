#include "lpv_gv_voxelizer.hpp"

#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <tl/optional.hpp>

#include "render/mesh_storage.hpp"
#include "render/render_scene.hpp"
#include "render/backend/pipeline_cache.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/render_graph.hpp"
#include "shared/triangle.hpp"

static_assert(sizeof(Triangle) == 36);

static std::shared_ptr<spdlog::logger> logger;

ThreeDeeRasterizer::ThreeDeeRasterizer(RenderBackend& backend_in) : backend{&backend_in} {
    if (logger == nullptr) {
        logger = SystemInterface::get().get_logger("3D Rasterizer");
    }

    auto& pipeline_cache = backend->get_pipeline_cache();

    texture_clear_shader = pipeline_cache.create_pipeline("shaders/voxelizer/clear.comp.spv");

    transform_verts_shader = pipeline_cache.create_pipeline("shaders/voxelizer/vertex_transformation.comp.spv");

    coarse_binning_shader = pipeline_cache.create_pipeline("shaders/voxelizer/binning_coarse.comp.spv");

    fine_binning_shader = pipeline_cache.create_pipeline("shaders/voxelizer/binning_fine.comp.spv");

    rasterize_primitives_shader = pipeline_cache.create_pipeline("shaders/voxelizer/rasterization.comp.spv");

    normalize_gv_shader = pipeline_cache.create_pipeline("shaders/voxelizer/normalize_sh.comp.spv");
}

ThreeDeeRasterizer::~ThreeDeeRasterizer() {
    auto& allocator = backend->get_global_allocator();
    deinit_resources(allocator);
}

void ThreeDeeRasterizer::init_resources(const glm::uvec3 voxel_texture_resolution, const uint32_t num_triangles) {
    resolution = voxel_texture_resolution;
    max_num_triangles = num_triangles;

    logger->debug(
        "Creating resources to rasterize {} triangles to a volume of resolution {}, {}, {}", num_triangles,
        voxel_texture_resolution.x, voxel_texture_resolution.y, voxel_texture_resolution.z
    );

    auto& allocator = backend->get_global_allocator();

    deinit_resources(allocator);

    const auto bin_resolution = (voxel_texture_resolution + glm::uvec3{3}) / glm::uvec3{4};

    const auto num_cells = voxel_texture_resolution.x * voxel_texture_resolution.y * voxel_texture_resolution.z;
    const auto num_bins = bin_resolution.x * bin_resolution.y * bin_resolution.z;
    const auto num_uints_per_bin = (max_num_triangles + 31) / 32;
    const auto num_coarse_uints_per_cell = (num_uints_per_bin + 31) / 32;

    voxel_texture = allocator.create_volume_texture(
        "Voxels", VK_FORMAT_R16G16B16A16_SFLOAT, voxel_texture_resolution, 1, TextureUsage::StorageImage
    );

    volume_uniform_buffer = allocator.create_buffer(
        "Voxel transform buffer", sizeof(glm::mat4), BufferUsage::UniformBuffer
    );

    const auto triangle_size = sizeof(Triangle);

    transformed_triangle_cache = allocator.create_buffer(
        "Transformed triangles", triangle_size * max_num_triangles, BufferUsage::StorageBuffer
    );

    triangle_sh_cache = allocator.create_buffer(
        "Triangles SH", sizeof(glm::vec4) * max_num_triangles, BufferUsage::StorageBuffer
    );

    bins = allocator.create_buffer(
        "Bin bitmask", sizeof(uint32_t) * num_uints_per_bin * num_bins, BufferUsage::StorageBuffer
    );

    cell_bitmask_coarse = allocator.create_buffer(
        "Coarse cell bitmask", sizeof(uint32_t) * num_coarse_uints_per_cell * num_cells, BufferUsage::StorageBuffer
    );

    cell_bitmask = allocator.create_buffer(
        "Cell bitmask", sizeof(uint32_t) * num_uints_per_bin * num_cells, BufferUsage::StorageBuffer
    );
}

void ThreeDeeRasterizer::deinit_resources(ResourceAllocator& allocator) {
    if (transformed_triangle_cache != BufferHandle::None) {
        allocator.destroy_buffer(transformed_triangle_cache);
        transformed_triangle_cache = BufferHandle::None;
    }
    if (bins != BufferHandle::None) {
        allocator.destroy_buffer(bins);
        bins = BufferHandle::None;
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

void ThreeDeeRasterizer::voxelize_mesh(RenderGraph& graph, const MeshHandle mesh, const MeshStorage& meshes) {
    const auto scale = mesh->bounds / 2.f;

    constexpr auto bias_mat = glm::mat4{
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.5f, 0.0f,
        0.5f, 0.5f, 0.5f, 1.0f
    };

    auto world_to_voxel = glm::mat4{1.f};
    world_to_voxel = glm::scale(world_to_voxel, glm::vec3{1.f} / scale);
    world_to_voxel = bias_mat * world_to_voxel;

    graph.add_pass(
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
                const auto set = *vkutil::DescriptorBuilder::begin(
                                      *backend, backend->get_transient_descriptor_allocator()
                                  )
                                  .bind_image(
                                      0, {.image = voxel_texture, .image_layout = VK_IMAGE_LAYOUT_GENERAL},
                                      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT
                                  )
                                  .build();

                commands.bind_descriptor_set(0, set);

                commands.bind_pipeline(texture_clear_shader);

                commands.dispatch((resolution.x + 3) / 4, (resolution.y + 3) / 4, (resolution.z + 3) / 4);

                commands.clear_descriptor_set(0);

                commands.update_buffer(volume_uniform_buffer, world_to_voxel);
            }
        }
    );

    const auto triangle_shader_set = *vkutil::DescriptorBuilder::begin(
                                          *backend, backend->get_transient_descriptor_allocator()
                                      )
                                      .bind_buffer(
                                          0, {.buffer = meshes.get_vertex_position_buffer()},
                                          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
                                      )
                                      .bind_buffer(
                                          1, {.buffer = meshes.get_vertex_data_buffer()},
                                          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
                                      )
                                      .bind_buffer(
                                          2, {.buffer = meshes.get_index_buffer()},
                                          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
                                      )
                                      .bind_buffer(
                                          3, {.buffer = volume_uniform_buffer},
                                          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
                                      )
                                      .bind_buffer(
                                          4, {.buffer = transformed_triangle_cache},
                                          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
                                      )

                                      .bind_buffer(
                                          5, {.buffer = triangle_sh_cache},
                                          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
                                      )
                                      .build();


    // TODO: The barrier for the transformed primitive cache should only synchronize the range this pass writes
    // to - not the whole buffer.
    // TODO: Add the ability to shade a subset of the triangles in a primitive
    graph.add_pass(
        ComputePass{
            .name = "Transform primitives",
            .buffers = {
                {
                    meshes.get_vertex_position_buffer(),
                    {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT}
                },
                {
                    meshes.get_vertex_data_buffer(),
                    {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT}
                },
                {
                    meshes.get_index_buffer(),
                    {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT}
                },
                {
                    volume_uniform_buffer,
                    {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_UNIFORM_READ_BIT}
                },
                {
                    transformed_triangle_cache,
                    {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT}
                },
                {
                    triangle_sh_cache,
                    {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT}
                },
            },
            .execute = [&, this](CommandBuffer& commands) {
                GpuZoneScopedN(commands, "Transform primitives")

                commands.bind_descriptor_set(0, triangle_shader_set);

                commands.bind_pipeline(transform_verts_shader);

                const auto num_triangles_to_shade = mesh->num_indices / 3;

                commands.set_push_constant(0, static_cast<uint32_t>(mesh->first_vertex));
                commands.set_push_constant(1, static_cast<uint32_t>(mesh->first_index));
                commands.set_push_constant(2, num_triangles_to_shade);

                logger->info("Rasterizing a mesh with {} triangles", num_triangles_to_shade);

                // / 96 because we have 96 threads per workgroup
                commands.dispatch((num_triangles_to_shade + 95) / 96, 1, 1);

                commands.clear_descriptor_set(0);
            }
        }
    );

    // Now that the triangle buffer is full, bin and rasterize the triangles
    graph.add_pass(
        {
            .name = "Bin triangles Low Res",
            .buffers = {
                {
                    transformed_triangle_cache,
                    {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT}
                },
                {
                    bins,
                    {
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT
                    }
                }
            },
            .execute = [&](CommandBuffer& commands) {
                GpuZoneScopedN(commands, "Bin Triangles Low Res")

                const auto set = *vkutil::DescriptorBuilder::begin(
                                      *backend, backend->get_transient_descriptor_allocator()
                                  )
                                  .bind_buffer(
                                      0, {.buffer = transformed_triangle_cache},
                                      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                      VK_SHADER_STAGE_COMPUTE_BIT
                                  )
                                  .bind_buffer(
                                      1, {
                                          .buffer = bins
                                      },
                                      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                      VK_SHADER_STAGE_COMPUTE_BIT
                                  )
                                  .build();

                commands.bind_descriptor_set(0, set);

                commands.bind_pipeline(coarse_binning_shader);

                const auto bin_resolution = (resolution + glm::uvec3{3}) / glm::uvec3{4};
                commands.dispatch(bin_resolution.x, bin_resolution.y, bin_resolution.z);

                commands.clear_descriptor_set(0);
            }
        }
    );

    graph.add_pass(
        {
            .name = "Bin Triangles High Res",
            .buffers = {
                {
                    transformed_triangle_cache,
                    {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT}
                },
                {
                    bins,
                    {
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_READ_BIT
                    }
                },
                {
                    cell_bitmask_coarse,
                    {
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT
                    }
                },
                {
                    cell_bitmask,
                    {
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT
                    }
                }
            },
            .execute = [&](CommandBuffer& commands) {
                GpuZoneScopedN(commands, "Bin Triangles High Res")

                const auto set = *vkutil::DescriptorBuilder::begin(
                                      *backend, backend->get_transient_descriptor_allocator()
                                  )
                                  .bind_buffer(
                                      0, {.buffer = transformed_triangle_cache},
                                      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                      VK_SHADER_STAGE_COMPUTE_BIT
                                  )
                                  .bind_buffer(
                                      1, {.buffer = bins},
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
                commands.bind_pipeline(fine_binning_shader);

                commands.set_push_constant(0, resolution.x);
                commands.set_push_constant(1, resolution.y);
                commands.set_push_constant(2, resolution.z);

                // Workgroups are 96 threads wide
                // commands.dispatch(resolution.x * 11, resolution.y, resolution.z);
                // Hack when debugging - use a smol dispatch because the prinf buffer is smol
                commands.dispatch(1, 1, 1);

                commands.clear_descriptor_set(0);
            }
        }

    );

    graph.add_pass(
        {
            .name = "Rasterize triangles",
            .textures = {
                {
                    voxel_texture,
                    {
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                        VK_IMAGE_LAYOUT_GENERAL
                    }
                }
            },
            .buffers = {
                {
                    triangle_sh_cache,
                    {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT}
                },
                {
                    cell_bitmask_coarse,
                    {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT}
                },
                {
                    cell_bitmask,
                    {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT}
                }
            },
            .execute = [&](CommandBuffer& commands) {
                GpuZoneScopedN(commands, "Rasterize triangles")
                const auto set = *vkutil::DescriptorBuilder::begin(
                                      *backend, backend->get_transient_descriptor_allocator()
                                  )
                                  .bind_buffer(
                                      0, {.buffer = triangle_sh_cache},
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

                commands.bind_pipeline(rasterize_primitives_shader);

                // TODO: Each thread should read one mask in the coarse bitmask - aka 32 masks in the fine bitmask

                commands.dispatch(resolution.x, resolution.y, resolution.z);

                commands.clear_descriptor_set(0);
            }
        }
    );
}

TextureHandle ThreeDeeRasterizer::extract_texture() {
    auto output_texture = voxel_texture;
    voxel_texture = TextureHandle::None;

    return output_texture;
}
