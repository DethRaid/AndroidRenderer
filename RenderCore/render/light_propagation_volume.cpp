#include "light_propagation_volume.hpp"

#include <magic_enum.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "backend/render_graph.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/resource_allocator.hpp"
#include "console/cvars.hpp"
#include "core/system_interface.hpp"
#include "render/scene_view.hpp"
#include "render/render_scene.hpp"
#include "render/mesh_storage.hpp"
#include "sdf/voxel_cache.hpp"
#include "shared/vpl.hpp"
#include "shared/lpv.hpp"

static auto cvar_lpv_resolution = AutoCVar_Int{
    "r.LPV.Resolution",
    "Resolution of one dimension of the light propagation volume", 32
};

static auto cvar_lpv_cell_size = AutoCVar_Float{
    "r.LPV.CellSize",
    "Size in meters of one size of a LPV cell", 0.25
};

static auto cvar_lpv_num_cascades = AutoCVar_Int{
    "r.LPV.NumCascades",
    "Number of cascades in the light propagation volume", 4
};

static auto cvar_lpv_num_propagation_steps = AutoCVar_Int{
    "r.LPV.NumPropagationSteps",
    "Number of times to propagate lighting through the LPV", 8
};

static auto cvar_lpv_behind_camera_percent = AutoCVar_Float{
    "r.LPV.PercentBehindCamera",
    "The percentage of the LPV that should be behind the camera. Not exact",
    0.2
};

static std::shared_ptr<spdlog::logger> logger;

LightPropagationVolume::LightPropagationVolume(RenderBackend& backend_in) : backend{backend_in} {
    if (logger == nullptr) {
        logger = SystemInterface::get().get_logger("LightPropagationVolume");
    }
    {
        vpl_pipeline = backend.begin_building_pipeline("RSM VPL extraction")
                              .set_vertex_shader("shaders/common/fullscreen.vert.spv")
                              .set_fragment_shader("shaders/lpv/rsm_generate_vpls.frag.spv")
                              .set_depth_state(
                                  DepthStencilState{.enable_depth_test = VK_FALSE, .enable_depth_write = VK_FALSE}
                              )
                              .build();
    }
    {
        const auto bytes = *SystemInterface::get().load_file("shaders/lpv/clear_lpv.comp.spv");
        clear_lpv_shader = *backend.create_compute_shader("Clear LPV", bytes);
    }
    {
        const auto bytes = *SystemInterface::get().load_file("shaders/lpv/inject_into_gv.comp.spv");
        inject_into_gv_shader = *backend.create_compute_shader("Inject into GV", bytes);
    }
    {
        vpl_injection_pipeline = backend.begin_building_pipeline("VPL Injection")
                                        .set_topology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
                                        .set_vertex_shader("shaders/lpv/vpl_injection.vert.spv")
                                        .set_geometry_shader("shaders/lpv/vpl_injection.geom.spv")
                                        .set_fragment_shader("shaders/lpv/vpl_injection.frag.spv")
                                        .set_blend_state(
                                            0, {
                                                .blendEnable = VK_TRUE,
                                                .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                                .colorBlendOp = VK_BLEND_OP_ADD,
                                                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                .alphaBlendOp = VK_BLEND_OP_ADD,
                                                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                                            }
                                        )
                                        .set_blend_state(
                                            1, {
                                                .blendEnable = VK_TRUE,
                                                .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                                .colorBlendOp = VK_BLEND_OP_ADD,
                                                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                .alphaBlendOp = VK_BLEND_OP_ADD,
                                                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                                            }
                                        )
                                        .set_blend_state(
                                            2, {
                                                .blendEnable = VK_TRUE,
                                                .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                                .colorBlendOp = VK_BLEND_OP_ADD,
                                                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                .alphaBlendOp = VK_BLEND_OP_ADD,
                                                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                                            }
                                        )
                                        .build();
    }
    {
        const auto bytes = *SystemInterface::get().load_file("shaders/lpv/lpv_propagate.comp.spv");
        propagation_shader = *backend.create_compute_shader("LPV Propagation", bytes);
    }

    lpv_render_shader = backend.begin_building_pipeline("LPV Rendering")
                               .set_vertex_shader("shaders/common/fullscreen.vert.spv")
                               .set_fragment_shader("shaders/lpv/overlay.frag.spv")
                               .set_depth_state({.enable_depth_test = VK_FALSE, .enable_depth_write = VK_FALSE})
                               .set_blend_state(
                                   0, {
                                       .blendEnable = VK_TRUE,
                                       .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                       .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                       .colorBlendOp = VK_BLEND_OP_ADD,
                                       .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                       .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                       .alphaBlendOp = VK_BLEND_OP_ADD,
                                       .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                       VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                                   }
                               )
                               .build();
}

void LightPropagationVolume::init_resources(ResourceAllocator& allocator) {
    const auto size = cvar_lpv_resolution.Get();
    const auto num_cascades = cvar_lpv_num_cascades.Get();

    const auto texture_resolution = glm::uvec3{size * num_cascades, size, size};

    lpv_a_red = allocator.create_volume_texture(
        "LPV Red A", VK_FORMAT_R16G16B16A16_SFLOAT,
        texture_resolution, 1, TextureUsage::StorageImage
    );
    lpv_a_green = allocator.create_volume_texture(
        "LPV Green A", VK_FORMAT_R16G16B16A16_SFLOAT,
        texture_resolution, 1, TextureUsage::StorageImage
    );
    lpv_a_blue = allocator.create_volume_texture(
        "LPV Blue A", VK_FORMAT_R16G16B16A16_SFLOAT,
        texture_resolution, 1, TextureUsage::StorageImage
    );
    lpv_b_red = allocator.create_volume_texture(
        "LPV Red B", VK_FORMAT_R16G16B16A16_SFLOAT,
        texture_resolution, 1, TextureUsage::StorageImage
    );
    lpv_b_green = allocator.create_volume_texture(
        "LPV Green B", VK_FORMAT_R16G16B16A16_SFLOAT,
        texture_resolution, 1, TextureUsage::StorageImage
    );
    lpv_b_blue = allocator.create_volume_texture(
        "LPV Blue B", VK_FORMAT_R16G16B16A16_SFLOAT,
        texture_resolution, 1, TextureUsage::StorageImage
    );

    geometry_volume_handle = allocator.create_volume_texture(
        "Geometry Volume", VK_FORMAT_R16G16B16A16_SFLOAT,
        texture_resolution, 1,
        TextureUsage::StorageImage
    );

    cascade_data_buffer = allocator.create_buffer(
        "LPV Cascade Data", sizeof(LPVCascadeMatrices) * num_cascades,
        BufferUsage::UniformBuffer
    );

    const auto num_cells = cvar_lpv_resolution.Get() * cvar_lpv_resolution.Get() * cvar_lpv_resolution.Get();

    cascades.resize(num_cascades);
    uint32_t cascade_index = 0;
    for (auto& cascade : cascades) {
        cascade.create_render_targets(allocator);
        cascade.count_buffer = allocator.create_buffer(
            fmt::format("Cascade {} VPL Count", cascade_index),
            sizeof(VkDrawIndirectCommand),
            BufferUsage::IndirectBuffer
        );
        cascade.vpl_buffer = allocator.create_buffer(
            fmt::format("Cascade {} VPL List", cascade_index),
            sizeof(PackedVPL) * 65536, BufferUsage::StorageBuffer
        );
        cascade_index++;
    }
}

void LightPropagationVolume::set_scene(RenderScene& scene_in, MeshStorage& meshes_in) {
    rsm_drawer = scene_in.create_view(ScenePassType::RSM, meshes_in);
}

void LightPropagationVolume::update_cascade_transforms(const SceneTransform& view, const SunLight& light) {
    const auto num_cells = cvar_lpv_resolution.Get();
    const auto base_cell_size = cvar_lpv_cell_size.GetFloat();

    const auto& view_position = view.get_position();

    // Position the LPV slightly in front of the view. We want some of the LPV to be behind it for reflections and such

    const auto num_cascades = cvar_lpv_num_cascades.Get();

    const auto offset_distance_scale = 0.5f - cvar_lpv_behind_camera_percent.GetFloat();

    for (uint32_t cascade_index = 0; cascade_index < num_cascades; cascade_index++) {
        const auto cell_size = base_cell_size * static_cast<float>(glm::pow(2.f, cascade_index));
        const auto cascade_size = num_cells * cell_size;

        // Offset the centerpoint of the cascade by 20% of the length of one side
        // When the camera is aligned with the X or Y axis, this will offset the cascade by 20% of its length. 30% of it
        // will be behind the camera, 70% of it will be in front. This feels reasonable
        // When the camera is 45 degrees off of the X or Y axis, the cascade will have more of itself behind the camera
        // This might be fine

        const auto offset_distance = cascade_size * offset_distance_scale;
        const auto offset = view_position + view.get_forward() * offset_distance;

        // Round to the cell size to prevent flickering
        const auto rounded_offset = glm::round(offset / cell_size) * cell_size;

        const auto scale_factor = 1.f / cascade_size;

        const auto bias_mat = glm::mat4{
            0.5f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.5f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.5f, 0.0f,
            0.5f, 0.5f, 0.5f, 1.0f
        };

        auto& cascade = cascades[cascade_index];
        cascade.world_to_cascade = glm::mat4{1.f};
        cascade.world_to_cascade = glm::scale(cascade.world_to_cascade, glm::vec3{scale_factor});
        cascade.world_to_cascade = glm::translate(cascade.world_to_cascade, -rounded_offset);
        cascade.world_to_cascade = bias_mat * cascade.world_to_cascade;

        const auto half_cascade_size = cascade_size / 2.f;
        constexpr auto rsm_pullback_distance = 32.f;
        const auto rsm_view_start = rounded_offset - light.get_direction() * rsm_pullback_distance;
        const auto rsm_view_matrix = glm::lookAt(rsm_view_start, rounded_offset, glm::vec3{0, 1, 0});
        const auto rsm_projection_matrix = glm::ortho(
            -half_cascade_size, half_cascade_size, -half_cascade_size, half_cascade_size, 0.f,
            rsm_pullback_distance * 2.f
        );
        cascade.rsm_vp = rsm_projection_matrix * rsm_view_matrix;

        cascade.min_bounds = rounded_offset - glm::vec3{half_cascade_size};
        cascade.max_bounds = rounded_offset + glm::vec3{half_cascade_size};
    }
}

void LightPropagationVolume::update_buffers(CommandBuffer& commands) const {
    auto cascade_matrices = std::vector<LPVCascadeMatrices>{};
    cascade_matrices.reserve(cascades.size());
    for (const auto& cascade : cascades) {
        cascade_matrices.push_back(
            {
                .rsm_vp = cascade.rsm_vp,
                .inverse_rsm_vp = glm::inverse(cascade.rsm_vp),
                .world_to_cascade = cascade.world_to_cascade,
            }
        );
    }

    commands.update_buffer(
        cascade_data_buffer, cascade_matrices.data(), cascade_matrices.size() * sizeof(LPVCascadeMatrices),
        0
    );
}

void LightPropagationVolume::inject_indirect_sun_light(
    RenderGraph& graph, RenderScene& scene
) {
    // For each LPV cascade:
    // Rasterize RSM render targets for the cascade, then render a fullscreen triangle over them. That triangle's FS
    // will select the brightest VPL in each subgroup, and write it to a buffer
    // Then, we dispatch one VS invocation for each VPL. We use a geometry shader to send the VPL to the correct part
    // of the cascade
    // Why do this? I want to keep the large, heavy RSM targets in tile memory. I have to use a FS for VPL extraction
    // because only a FS can read from tile memory. I reduce the 1024x1024 RSM to only 65k VPLs, so there's much less
    // data flushed to main memory
    // Unfortunately there's a sync point between the VPL generation FS and the VPL injection pass. Not sure I can get
    // rid of that

    graph.begin_label("LPV indirect sun light injection");

    auto cascade_index = 0u;
    for (const auto& cascade : cascades) {
        graph.add_compute_pass(
            {
                .name = "Clear count buffer",
                .buffers = {
                    {
                        cascade.count_buffer,
                        {.stage = VK_PIPELINE_STAGE_TRANSFER_BIT, .access = VK_ACCESS_TRANSFER_WRITE_BIT}
                    }
                },
                .execute = [&](CommandBuffer& commands) { commands.fill_buffer(cascade.count_buffer, 0); }
            }
        );

        graph.add_render_pass(
            RenderPass{
                .name = "Render RSM and generate VPLs",
                .buffers = {
                    {cascade.count_buffer, {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT}},
                    {cascade.vpl_buffer, {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT}},
                },
                .render_targets = {
                    cascade.flux_target,
                    cascade.normals_target,
                    cascade.depth_target,
                },
                .clear_values = {
                    VkClearValue{.color = {.float32 = {0, 0, 0, 0}}},
                    VkClearValue{.color = {.float32 = {0.5, 0.5, 1.0, 0}}},
                    VkClearValue{.depthStencil = {.depth = 1.f}}
                },
                .subpasses = {
                    Subpass{
                        .name = "RSM",
                        .color_attachments = {0, 1},
                        .depth_attachment = 2,
                        .execute = [&](CommandBuffer& commands) {
                            GpuZoneScopedN(commands, "Render RSM")
                            auto global_set = *backend.create_frame_descriptor_builder()
                                                      .bind_buffer(
                                                          0, {.buffer = cascade_data_buffer},
                                                          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                          VK_SHADER_STAGE_VERTEX_BIT
                                                      )
                                                      .bind_buffer(
                                                          1, {.buffer = scene.get_sun_light().get_constant_buffer()},
                                                          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                          VK_SHADER_STAGE_FRAGMENT_BIT
                                                      )
                                                      .build();

                            commands.bind_descriptor_set(0, global_set);

                            commands.set_push_constant(1, cascade_index);

                            rsm_drawer.draw(commands);

                            commands.clear_descriptor_set(0);
                        }
                    },
                    Subpass{
                        .name = "VPL Generation",
                        .input_attachments = {0, 1, 2},
                        .execute = [&](CommandBuffer& commands) {
                            GpuZoneScopedN(commands, "VPL Generation")
                            const auto sampler = backend.get_default_sampler();

                            const auto set = backend.create_frame_descriptor_builder()
                                                    .bind_image(
                                                        0,
                                                        {
                                                            .sampler = sampler, .image = cascade.flux_target,
                                                            .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                                        },
                                                        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT
                                                    )
                                                    .bind_image(
                                                        1,
                                                        {
                                                            .sampler = sampler, .image = cascade.normals_target,
                                                            .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                                        },
                                                        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT
                                                    )
                                                    .bind_image(
                                                        2,
                                                        {
                                                            .sampler = sampler, .image = cascade.depth_target,
                                                            .image_layout =
                                                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                                        },
                                                        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT
                                                    )
                                                    .bind_buffer(
                                                        3, {.buffer = cascade_data_buffer},
                                                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT
                                                    )
                                                    .bind_buffer(
                                                        4, {.buffer = cascade.count_buffer},
                                                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT
                                                    )
                                                    .bind_buffer(
                                                        5, {.buffer = cascade.vpl_buffer},
                                                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT
                                                    )
                                                    .build();

                            commands.bind_descriptor_set(0, *set);

                            commands.set_push_constant(0, cascade_index);

                            commands.bind_pipeline(vpl_pipeline);

                            commands.draw_triangle();

                            commands.clear_descriptor_set(0);
                        },
                    }
                }
            }
        );

        graph.add_render_pass(
            {
                .name = "VPL Injection",
                .buffers = {
                    {cascade_data_buffer, {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT}},
                    {cascade.vpl_buffer, {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT}},
                    {cascade.count_buffer, {VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT}},
                },
                .render_targets = {lpv_a_red, lpv_a_green, lpv_a_blue},
                .subpasses = {
                    {
                        .name = "VPL Injection",
                        .color_attachments = {0, 1, 2},
                        .execute = [&](CommandBuffer& commands) {
                            GpuZoneScopedN(commands, "VPL Injection")

                            const auto set = *backend.create_frame_descriptor_builder()
                                                     .bind_buffer(
                                                         0, {.buffer = cascade_data_buffer},
                                                         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                         VK_SHADER_STAGE_VERTEX_BIT
                                                     )
                                                     .bind_buffer(
                                                         1, {.buffer = cascade.vpl_buffer},
                                                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT
                                                     )
                                                     .build();

                            commands.bind_descriptor_set(0, set);

                            commands.set_push_constant(0, cascade_index);

                            commands.bind_pipeline(vpl_injection_pipeline);

                            commands.draw_indirect(cascade.count_buffer);

                            commands.clear_descriptor_set(0);
                        }
                    }
                }
            }
        );

        // graph.add_render_pass(
        //     RenderPass{
        //         .name = "Inject RSM geometry",
        //         .textures = {
        //             {
        //                 cascade.normals_target,
        //                 {
        //                     VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        //                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        //                 }
        //             },
        //             {
        //                 cascade.depth_target,
        //                 {
        //                     VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        //                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        //                 }
        //             }
        //         },
        //         .render_targets = {geometry_volume_handle},
        //         .subpasses = {
        //             {
        //                 .name = "Inject RSM geometry into GV",
        //                 .color_attachments = {0},
        //                 .execute = [&](CommandBuffer& commands) {}
        //             }
        //         }
        //     }
        // );

        cascade_index++;
    }

    graph.end_label();
}

void LightPropagationVolume::clear_volume(RenderGraph& render_graph) {
    render_graph.add_compute_pass(
        {
            .name = "LightPropagationVolume::clear_volume",
            .textures = {
                {
                    lpv_a_red,
                    {
                        .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, .access = VK_ACCESS_2_SHADER_WRITE_BIT,
                        .layout = VK_IMAGE_LAYOUT_GENERAL
                    }
                },
                {
                    lpv_a_green,
                    {
                        .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, .access = VK_ACCESS_2_SHADER_WRITE_BIT,
                        .layout = VK_IMAGE_LAYOUT_GENERAL
                    }
                },
                {
                    lpv_a_blue,
                    {
                        .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, .access = VK_ACCESS_2_SHADER_WRITE_BIT,
                        .layout = VK_IMAGE_LAYOUT_GENERAL
                    }
                },
                {
                    geometry_volume_handle,
                    {
                        .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, .access = VK_ACCESS_2_SHADER_WRITE_BIT,
                        .layout = VK_IMAGE_LAYOUT_GENERAL,
                    }
                }
            },
            .execute = [&](CommandBuffer& commands) {
                auto descriptor_set = *backend.create_frame_descriptor_builder()
                                              .bind_image(
                                                  0, {
                                                      .image = lpv_a_red,
                                                      .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                                  },
                                                  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                  VK_SHADER_STAGE_COMPUTE_BIT
                                              )
                                              .bind_image(
                                                  1, {
                                                      .image = lpv_a_green,
                                                      .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                                  },
                                                  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                  VK_SHADER_STAGE_COMPUTE_BIT
                                              )
                                              .bind_image(
                                                  2, {
                                                      .image = lpv_a_blue,
                                                      .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                                  },
                                                  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                  VK_SHADER_STAGE_COMPUTE_BIT
                                              )
                                              .bind_image(
                                                  3, {
                                                      .image = geometry_volume_handle,
                                                      .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                                  },
                                                  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                  VK_SHADER_STAGE_COMPUTE_BIT
                                              )
                                              .build();

                commands.bind_descriptor_set(0, descriptor_set);

                commands.bind_shader(clear_lpv_shader);

                commands.dispatch(cvar_lpv_num_cascades.Get(), 32, 32);

                commands.clear_descriptor_set(0);
            }
        }
    );
}

void LightPropagationVolume::build_geometry_volume(
    RenderGraph& render_graph, const RenderScene& scene, const VoxelCache& voxel_cache
) {
    /*
     * For each cascade:
     * - Get the static mesh primitives in the cascade
     * - Dispatch a compute shader to add them to the GV
     */

    auto& allocator = backend.get_global_allocator();

    const auto num_cascades = cvar_lpv_num_cascades.Get();
    for (auto cascade_idx = 0u; cascade_idx < num_cascades; cascade_idx++) {
        const auto& cascade = cascades[cascade_idx];
        const auto& primitives = scene.get_primitives_in_bounds(cascade.min_bounds, cascade.max_bounds);

        if (primitives.empty()) {
            continue;
        }

        // Two arrays: one for the voxels for each primitive, one that maps from thread ID to primitive ID
        auto primitive_ids = std::vector<uint32_t>{};
        auto textures = std::vector<vkutil::DescriptorBuilder::ImageInfo>{};
        primitive_ids.reserve(primitives.size());
        textures.reserve(primitives.size());

        for (const auto& primitive : primitives) {
            primitive_ids.push_back(primitive.index);
            const auto& mesh = primitive->mesh;
            const auto& voxel = voxel_cache.get_voxel_for_mesh(mesh);
            textures.emplace_back(
                vkutil::DescriptorBuilder::ImageInfo{
                    .sampler = backend.get_default_sampler(),
                    .image = voxel.sh_texture,
                    .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                }
            );
        }

        const auto primitive_id_buffer = allocator.create_buffer(
            "Primitive ID buffer", primitive_ids.size() * sizeof(uint32_t), BufferUsage::UniformBuffer
        );

        render_graph.add_compute_pass(
            {
                .name = "Voxel injection",
                .textures = {
                    {
                        geometry_volume_handle,
                        {
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                            VK_IMAGE_LAYOUT_GENERAL
                        },
                    }
                },
                .buffers = {
                    {primitive_id_buffer, {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_UNIFORM_READ_BIT}}
                },
                .execute = [&, primitive_ids = std::move(primitive_ids), textures = std::move(textures)](
                CommandBuffer& commands
            ) {
                    commands.update_buffer(
                        primitive_id_buffer, primitive_ids.data(), primitive_ids.size() * sizeof(uint32_t), 0
                    );

                    const auto set = *backend.create_frame_descriptor_builder()
                                             .bind_buffer(
                                                 0, {.buffer = cascade_data_buffer}, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                 VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .bind_buffer(
                                                 1, {.buffer = primitive_id_buffer}, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                 VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .bind_buffer(
                                                 2, {.buffer = scene.get_primitive_buffer()},
                                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .bind_image(
                                                 3, {
                                                     .image = geometry_volume_handle,
                                                     .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                                 }, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .bind_image_array(
                                                 4, textures, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                 VK_SHADER_STAGE_COMPUTE_BIT
                                             )
                                             .build();

                    commands.bind_descriptor_set(0, set);
                    commands.set_push_constant(0, static_cast<uint32_t>(textures.size()));
                    commands.set_push_constant(1, cascade_idx);
                    commands.bind_shader(inject_into_gv_shader);

                    commands.dispatch(32, 32, 32);

                    commands.clear_descriptor_set(0);
                }
            }
        );

        allocator.destroy_buffer(primitive_id_buffer);
    }
}

void LightPropagationVolume::propagate_lighting(RenderGraph& render_graph) {
    render_graph.begin_label("LPV Propagation");

    for (auto step_index = 0; step_index < cvar_lpv_num_propagation_steps.Get(); step_index += 2) {
        perform_propagation_step(
            render_graph, lpv_a_red, lpv_a_green, lpv_a_blue, lpv_b_red, lpv_b_green, lpv_b_blue
        );
        perform_propagation_step(
            render_graph, lpv_b_red, lpv_b_green, lpv_b_blue, lpv_a_red, lpv_a_green, lpv_a_blue
        );
    }

    render_graph.add_transition_pass(
        {
            .textures = {
                {
                    lpv_a_red, {
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    }
                },
                {
                    lpv_a_green, {
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    }
                },
                {
                    lpv_a_blue, {
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    }
                }
            }
        }
    );

    render_graph.end_label();
}

void LightPropagationVolume::add_lighting_to_scene(
    CommandBuffer& commands, VkDescriptorSet gbuffers_descriptor, const BufferHandle scene_view_buffer
) {
    GpuZoneScoped(commands)

    commands.begin_label("LightPropagationVolume::add_lighting_to_scene");

    commands.bind_descriptor_set(0, gbuffers_descriptor);

    const auto sampler = backend.get_global_allocator().get_sampler(
        {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = 16,
            .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK
        }
    );

    auto lpv_descriptor = *backend.create_frame_descriptor_builder()
                                  .bind_image(
                                      0, {
                                          .sampler = sampler, .image = lpv_a_red,
                                          .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                      },
                                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT
                                  )
                                  .bind_image(
                                      1, {
                                          .sampler = sampler, .image = lpv_a_green,
                                          .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                      },
                                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT
                                  )
                                  .bind_image(
                                      2, {
                                          .sampler = sampler, .image = lpv_a_blue,
                                          .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                      },
                                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT
                                  )
                                  .bind_buffer(
                                      3, {.buffer = cascade_data_buffer}, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                      VK_SHADER_STAGE_FRAGMENT_BIT
                                  )
                                  .bind_buffer(
                                      4, {.buffer = scene_view_buffer}, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                      VK_SHADER_STAGE_FRAGMENT_BIT
                                  )
                                  .build();
    commands.bind_descriptor_set(1, lpv_descriptor);

    commands.bind_pipeline(lpv_render_shader);

    commands.draw_triangle();

    commands.clear_descriptor_set(1);
    commands.clear_descriptor_set(0);

    commands.end_label();
}

void LightPropagationVolume::perform_propagation_step(
    RenderGraph& render_graph,
    const TextureHandle read_red, const TextureHandle read_green, const TextureHandle read_blue,
    const TextureHandle write_red, const TextureHandle write_green, const TextureHandle write_blue
) const {
    render_graph.add_compute_pass(
        {
            .name = "Propagate lighting",
            .textures = {
                {
                    read_red,
                    {
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_GENERAL
                    }
                },
                {
                    read_green,
                    {
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_GENERAL
                    }
                },
                {
                    read_blue,
                    {
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_GENERAL
                    }
                },
                {
                    write_red,
                    {
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                        VK_IMAGE_LAYOUT_GENERAL
                    }
                },
                {
                    write_green,
                    {
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                        VK_IMAGE_LAYOUT_GENERAL
                    }
                },
                {
                    write_blue,
                    {
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                        VK_IMAGE_LAYOUT_GENERAL
                    }
                },
                {
                    geometry_volume_handle,
                    {
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_GENERAL
                    }
                },
            },
            .execute = [&](CommandBuffer& commands) {
                const auto descriptor_set = *backend.create_frame_descriptor_builder()
                                                    .bind_image(
                                                        0, {
                                                            .image = read_red,
                                                            .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                                        },
                                                        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                        VK_SHADER_STAGE_COMPUTE_BIT
                                                    )
                                                    .bind_image(
                                                        1, {
                                                            .image = read_green,
                                                            .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                                        },
                                                        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                        VK_SHADER_STAGE_COMPUTE_BIT
                                                    )
                                                    .bind_image(
                                                        2, {
                                                            .image = read_blue,
                                                            .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                                        },
                                                        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                        VK_SHADER_STAGE_COMPUTE_BIT
                                                    )
                                                    .bind_image(
                                                        3, {
                                                            .image = write_red,
                                                            .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                                        },
                                                        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                        VK_SHADER_STAGE_COMPUTE_BIT
                                                    )
                                                    .bind_image(
                                                        4, {
                                                            .image = write_green,
                                                            .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                                        },
                                                        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                        VK_SHADER_STAGE_COMPUTE_BIT
                                                    )
                                                    .bind_image(
                                                        5, {
                                                            .image = write_blue,
                                                            .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                                        },
                                                        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                        VK_SHADER_STAGE_COMPUTE_BIT
                                                    )
                                                    .bind_image(
                                                        6, {
                                                            .image = geometry_volume_handle,
                                                            .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                                        },
                                                        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                        VK_SHADER_STAGE_COMPUTE_BIT
                                                    )
                                                    .build();

                commands.bind_descriptor_set(0, descriptor_set);

                commands.bind_shader(propagation_shader);

                for (uint32_t cascade_index = 0; cascade_index < cvar_lpv_num_cascades.Get(); cascade_index++) {
                    commands.set_push_constant(0, cascade_index);

                    commands.dispatch(1, 32, 32);
                }

                commands.clear_descriptor_set(0);
            }
        }
    );
}

void CascadeData::create_render_targets(ResourceAllocator& allocator) {
    flux_target = allocator.create_texture(
        "RSM Flux", VK_FORMAT_R8G8B8A8_SRGB, glm::uvec2{1024}, 1,
        TextureUsage::RenderTarget
    );
    normals_target = allocator.create_texture(
        "RSM Normals", VK_FORMAT_R8G8B8A8_UNORM, glm::uvec2{1024}, 1,
        TextureUsage::RenderTarget
    );
    depth_target = allocator.create_texture(
        "RSM Depth", VK_FORMAT_D16_UNORM, glm::uvec2{1024}, 1,
        TextureUsage::RenderTarget
    );
}
