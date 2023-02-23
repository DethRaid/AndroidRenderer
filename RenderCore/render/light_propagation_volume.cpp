#include "light_propagation_volume.hpp"

#include "backend/render_graph.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/resource_allocator.hpp"
#include "console/cvars.hpp"
#include "core/system_interface.hpp"
#include "render/scene_view.hpp"
#include "glm/ext/matrix_transform.hpp"

static auto cvar_lpv_resolution = AutoCVar_Int{
    "r.LPV.Resolution",
    "Resolution of one dimension of the light propagation volume", 32
};

static auto cvar_lpv_cell_size = AutoCVar_Float{
    "r.LPV.CellSize",
    "Size in meters of one size of a LPV cell", 0.5
};

static auto cvar_lpv_num_cascades = AutoCVar_Int{
    "r.LPV.NumCascades",
    "Number of cascades in the light propagation volume", 1
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

LightPropagationVolume::LightPropagationVolume(RenderBackend& backend_in) : backend{backend_in} {
    {
        const auto bytes = *SystemInterface::get().load_file("shaders/lpv/clear_lpv.comp.spv");
        clear_lpv_shader = *backend.create_compute_shader("Clear LPV", bytes);
    }
    {
        const auto bytes = *SystemInterface::get().load_file("shaders/lpv/vpl_placement.comp.spv");
        vpl_placement_shader = *backend.create_compute_shader("VPL Placement", bytes);
    }
    {
        const auto bytes = *SystemInterface::get().load_file("shaders/lpv/vpl_injection.comp.spv");
        vpl_injection_shader = *backend.create_compute_shader("VPL Injection", bytes);
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
        "LPV Cascade Data", sizeof(glm::mat4) * num_cascades,
        BufferUsage::UniformBuffer
    );

    cascades.resize(num_cascades);
    uint32_t cascade_index = 0;
    for (auto& cascade : cascades) {
        cascade.vpl_list = allocator.create_buffer(
            fmt::format("Cascade {} VPL List", cascade_index),
            sizeof(glm::uvec2) * 65536, BufferUsage::StorageBuffer
        );
        cascade.vpl_list_count = allocator.create_buffer(
            fmt::format("Cascade {} VPL Count", cascade_index),
            sizeof(uint32_t), BufferUsage::StorageBuffer
        );
        cascade.vpl_list_head = allocator.create_buffer(
            fmt::format("Cascade {} VPL List Head", cascade_index),
            sizeof(uint32_t) * 32 * 32 * 32, BufferUsage::StorageBuffer
        );
        cascade_index++;
    }
}

void LightPropagationVolume::update_cascade_transforms(const SceneView& view) {
    const auto num_cells = cvar_lpv_resolution.Get();
    const auto base_cell_size = cvar_lpv_cell_size.GetFloat();

    const auto& view_position = view.get_postion();

    // Position the LPV slightly in front of the view. We want some of the LPV to be behind it for reflections and such

    const auto num_cascades = cvar_lpv_num_cascades.Get();

    const auto offset_distance_scale = 0.5f - cvar_lpv_behind_camera_percent.GetFloat();

    const auto bias_mat = glm::mat4{
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.5f, 0.0f,
        0.5f, 0.5f, 0.5f, 0.0f,
    };

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

        auto& cascade = cascades[cascade_index];
        cascade.world_to_cascade = glm::mat4{1.f};
        cascade.world_to_cascade = glm::scale(cascade.world_to_cascade, glm::vec3{scale_factor});
        cascade.world_to_cascade = glm::translate(cascade.world_to_cascade, -rounded_offset);
        cascade.world_to_cascade = bias_mat * cascade.world_to_cascade;
    }
}

void LightPropagationVolume::add_clear_volume_pass(RenderGraph& render_graph) {
    render_graph.add_compute_pass(
        {
            .name = "LightPropagationVolume::clear_volume",
            .textures = {
                {
                    lpv_a_red,
                    {
                        .stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, .access = VK_ACCESS_SHADER_WRITE_BIT,
                        .layout = VK_IMAGE_LAYOUT_GENERAL
                    }
                },
                {
                    lpv_a_green,
                    {
                        .stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, .access = VK_ACCESS_SHADER_WRITE_BIT,
                        .layout = VK_IMAGE_LAYOUT_GENERAL
                    }
                },
                {
                    lpv_a_blue,
                    {
                        .stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, .access = VK_ACCESS_SHADER_WRITE_BIT,
                        .layout = VK_IMAGE_LAYOUT_GENERAL
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
                                                  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT
                                              )
                                              .bind_image(
                                                  1, {
                                                      .image = lpv_a_green,
                                                      .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                                  },
                                                  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT
                                              )
                                              .bind_image(
                                                  2, {
                                                      .image = lpv_a_blue,
                                                      .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                                  },
                                                  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT
                                              )
                                              .build();

                commands.bind_descriptor_set(0, descriptor_set);

                commands.bind_shader(clear_lpv_shader);

                commands.dispatch(4, 32, 32);

                commands.clear_descriptor_set(0);
            }
        }

    );
}

void LightPropagationVolume::inject_lights(RenderGraph& render_graph, BufferHandle vpl_list_buffer) const {
    render_graph.add_transition_pass(
        {
            .buffers = {
                {
                    vpl_list_buffer,
                    {.stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, .access = VK_ACCESS_SHADER_READ_BIT}
                }
            }
        }
    );

    const auto num_cascades = cvar_lpv_num_cascades.Get();


    // This is hard
    // For each cascade:
    // - Dispatch a compute shader over the lights. Transform the lights into cascade space, add them to the linked list
    //      of lights for the cell they're in
    // - Dispatch a compute shader over the cascade. Read all the lights from the current cell's light list and add them
    //      to the cascade

    // Build a per-cell linked list of lights
    for (uint32_t cascade_index = 0; cascade_index < num_cascades; cascade_index++) {
        const auto& cascade = cascades[cascade_index];

        render_graph.add_compute_pass(
            {
                .name = fmt::format("Clear light list: cascade {}", cascade_index),
                .buffers = {
                    {
                        cascade.vpl_list_count,
                        {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT}
                    },
                    {cascade.vpl_list_head, {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT}},
                },
                .execute = [&](CommandBuffer& commands) {
                    commands.fill_buffer(cascade.vpl_list_count, 0xFFFFFFFF);
                    commands.fill_buffer(cascade.vpl_list_head, 0xFFFFFFFF);
                }
            }
        );

        render_graph.add_compute_pass(
            {
                .name = fmt::format("Build light list: cascade {}", cascade_index),
                .buffers = {
                    {
                        cascade.vpl_list,
                        BufferUsageToken{
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
                        }
                    },
                    {
                        cascade.vpl_list_count,
                        BufferUsageToken{
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
                        }
                    },
                    {
                        cascade.vpl_list_head,
                        BufferUsageToken{
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
                        }
                    }
                },
                .execute = [&](CommandBuffer& commands) {
                    auto descriptor_set = *backend.create_frame_descriptor_builder()
                                                  .bind_buffer(
                                                      0, {.buffer = vpl_list_buffer},
                                                      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                      VK_SHADER_STAGE_COMPUTE_BIT
                                                  )
                                                  .bind_buffer(
                                                      1, {.buffer = cascade_data_buffer},
                                                      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                      VK_SHADER_STAGE_COMPUTE_BIT
                                                  )
                                                  .bind_buffer(
                                                      2, {.buffer = cascade.vpl_list},
                                                      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                      VK_SHADER_STAGE_COMPUTE_BIT
                                                  )
                                                  .bind_buffer(
                                                      3, {.buffer = cascade.vpl_list_count},
                                                      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                      VK_SHADER_STAGE_COMPUTE_BIT
                                                  )
                                                  .bind_buffer(
                                                      4, {.buffer = cascade.vpl_list_head},
                                                      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                      VK_SHADER_STAGE_COMPUTE_BIT
                                                  )
                                                  .build();

                    commands.bind_descriptor_set(0, descriptor_set);

                    commands.set_push_constant(0, cascade_index);

                    commands.bind_shader(vpl_placement_shader);

                    commands.dispatch(65536 / 32, 1, 1);

                    commands.clear_descriptor_set(0);

                    commands.end_label();
                }
            }
        );
    }

    // Transition the images ahead of time so all the cascades can execute together

    render_graph.add_transition_pass(
        {
            .textures = {
                {
                    lpv_a_red, {
                        .stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        .access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                        .layout = VK_IMAGE_LAYOUT_GENERAL
                    },
                },
                {
                    lpv_a_green, {
                        .stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        .access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                        .layout = VK_IMAGE_LAYOUT_GENERAL
                    },
                },
                {
                    lpv_a_blue, {
                        .stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        .access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                        .layout = VK_IMAGE_LAYOUT_GENERAL
                    }
                }
            }
        }
    );

    for (uint32_t cascade_index = 0; cascade_index < num_cascades; cascade_index++) {
        // Walk the linked list and add the lights to the LPV
        const auto& cascade = cascades[cascade_index];

        render_graph.add_compute_pass(
            {
                .name = fmt::format("Inject VPLs into cascade {}", cascade_index),
                .buffers = {
                    {cascade.vpl_list, {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT}},
                    {
                        cascade.vpl_list_head,
                        {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT}
                    }
                },
                .execute = [&](CommandBuffer& commands) {
                    auto descriptor_set = *backend.create_frame_descriptor_builder()
                                                  .bind_buffer(
                                                      0, {.buffer = vpl_list_buffer},
                                                      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                      VK_SHADER_STAGE_COMPUTE_BIT
                                                  )
                                                  .bind_buffer(
                                                      1, {.buffer = cascade_data_buffer},
                                                      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                      VK_SHADER_STAGE_COMPUTE_BIT
                                                  )
                                                  .bind_buffer(
                                                      2, {.buffer = cascade.vpl_list},
                                                      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                      VK_SHADER_STAGE_COMPUTE_BIT
                                                  )
                                                  .bind_buffer(
                                                      3, {.buffer = cascade.vpl_list_head},
                                                      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                      VK_SHADER_STAGE_COMPUTE_BIT
                                                  )
                                                  .bind_image(
                                                      4, {
                                                          .image = lpv_a_red,
                                                          .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                                      },
                                                      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                      VK_SHADER_STAGE_COMPUTE_BIT
                                                  )
                                                  .bind_image(
                                                      5, {
                                                          .image = lpv_a_green,
                                                          .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                                      },
                                                      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                      VK_SHADER_STAGE_COMPUTE_BIT
                                                  )
                                                  .bind_image(
                                                      6, {
                                                          .image = lpv_a_blue,
                                                          .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                                      },
                                                      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                      VK_SHADER_STAGE_COMPUTE_BIT
                                                  )
                                                  .build();

                    commands.bind_descriptor_set(0, descriptor_set);

                    commands.set_push_constant(0, cascade_index);

                    commands.bind_shader(vpl_injection_shader);

                    commands.dispatch(1, 32, 32);

                    commands.clear_descriptor_set(0);
                }
            }
        );
    }
}

void LightPropagationVolume::propagate_lighting(RenderGraph& render_graph) {
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
}

void LightPropagationVolume::add_lighting_to_scene(
    CommandBuffer& commands, VkDescriptorSet gbuffers_descriptor, const BufferHandle scene_view_buffer
) {
    ZoneScoped;
    GpuZoneScoped(commands);

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
                                                    .build();

                commands.bind_descriptor_set(0, descriptor_set);

                commands.bind_shader(propagation_shader);

                for (uint32_t cascade_index = 0; cascade_index < cvar_lpv_num_cascades.Get(); cascade_index
                     ++) {
                    commands.set_push_constant(0, cascade_index);

                    commands.dispatch(1, 32, 32);
                }

                commands.clear_descriptor_set(0);
            }
        }
    );
}

void LightPropagationVolume::update_buffers(CommandBuffer& commands) const {
    auto cascade_matrices = std::vector<glm::mat4>{};
    cascade_matrices.reserve(cascades.size());
    for (const auto& cascade : cascades) {
        cascade_matrices.push_back(cascade.world_to_cascade);
    }

    commands.update_buffer(
        cascade_data_buffer, cascade_matrices.data(), cascade_matrices.size() * sizeof(glm::mat4), 0
    );
}
