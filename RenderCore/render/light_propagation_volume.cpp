#include "light_propagation_volume.hpp"

#include <glm/ext/matrix_transform.hpp>

#include "backend/render_graph.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/resource_allocator.hpp"
#include "console/cvars.hpp"
#include "core/system_interface.hpp"
#include "render/scene_view.hpp"
#include "render/render_scene.hpp"
#include "render/mesh_storage.hpp"
#include "shared/vpl.hpp"
#include "shared/lpv.hpp"

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

    // RSM + VPL
    {
        const auto attachments = std::array{
            // RSM flux
            VkAttachmentDescription{
                .format = VK_FORMAT_R8G8B8A8_SRGB,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            },

            // RSM normals
            VkAttachmentDescription{
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            },

            // Depth buffer
            VkAttachmentDescription{
                .format = VK_FORMAT_D16_UNORM,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            }
        };

        const auto rsm_attachments = std::array{
            // Flux
            VkAttachmentReference{
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            // Normals
            VkAttachmentReference{
                .attachment = 1,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            }
        };
        const auto depth_attachment = VkAttachmentReference{
            .attachment = 2,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };

        const auto vpl_input_attachments = std::array{
            // Flux
            VkAttachmentReference{
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            // Normals
            VkAttachmentReference{
                .attachment = 1,
                .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            // Depth
            VkAttachmentReference{
                .attachment = 2,
                .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            }
        };

        const auto subpasses = std::array{
            // Shadow + RSM
            VkSubpassDescription{
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = static_cast<uint32_t>(rsm_attachments.size()),
                .pColorAttachments = rsm_attachments.data(),
                .pDepthStencilAttachment = &depth_attachment,
            },

            // VPL list extraction
            VkSubpassDescription{
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .inputAttachmentCount = static_cast<uint32_t>(vpl_input_attachments.size()),
                .pInputAttachments = vpl_input_attachments.data(),
            }
        };

        const auto dependencies = std::array{
            // VPL depends on RSM
            VkSubpassDependency{
                .srcSubpass = 0,
                .dstSubpass = 1,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            }
        };

        const auto create_info = VkRenderPassCreateInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),
            .subpassCount = static_cast<uint32_t>(subpasses.size()),
            .pSubpasses = subpasses.data(),
            .dependencyCount = static_cast<uint32_t>(dependencies.size()),
            .pDependencies = dependencies.data()
        };

        const auto result = vkCreateRenderPass(backend.get_device().device, &create_info, nullptr, &rsm_render_pass);
        if (result != VK_SUCCESS) {
            logger->error("Could not create RSM renderpass. Vulkan error {}", result);
        } else {
            logger->info("RSM renderpass created!");
        }
    }
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
    for (auto& cascade: cascades) {
        cascade.create_render_targets(allocator);
        cascade.count_buffer = allocator.create_buffer(fmt::format("Cascade {} VPL Count", cascade_index),
                                                       sizeof(uint32_t),
                                                       BufferUsage::StorageBuffer);
        cascade.vpl_buffer = allocator.create_buffer(fmt::format("Cascade {} VPL List", cascade_index),
                                                     sizeof(PackedVPL) * 65536, BufferUsage::StorageBuffer);
        cascade.vpl_list = allocator.create_buffer(
            fmt::format("Cascade {} VPL Linked List", cascade_index),
            sizeof(glm::uvec2) * 65536, BufferUsage::StorageBuffer
        );
        cascade.vpl_list_count = allocator.create_buffer(
            fmt::format("Cascade {} VPL Linked List Bump Point", cascade_index),
            sizeof(uint32_t), BufferUsage::StorageBuffer
        );
        cascade.vpl_list_head = allocator.create_buffer(
            fmt::format("Cascade {} VPL Linked List Head", cascade_index),
            sizeof(uint32_t) * num_cells, BufferUsage::StorageBuffer
        );
        cascade_index++;
    }
}

void LightPropagationVolume::update_cascade_transforms(const SceneView& view, const SunLight& light) {
    const auto num_cells = cvar_lpv_resolution.Get();
    const auto base_cell_size = cvar_lpv_cell_size.GetFloat();

    const auto& view_position = view.get_postion();

    // Position the LPV slightly in front of the view. We want some of the LPV to be behind it for reflections and such

    const auto num_cascades = cvar_lpv_num_cascades.Get();

    const auto offset_distance_scale = 0.5f - cvar_lpv_behind_camera_percent.GetFloat();
    
    for (uint32_t cascade_index = 0 ; cascade_index < num_cascades ; cascade_index++) {
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
            0.5f, 0.5f, 0.5f, 0.0f
        };

        auto& cascade = cascades[cascade_index];
        cascade.world_to_cascade = glm::mat4{1.f};
        cascade.world_to_cascade = glm::scale(cascade.world_to_cascade, glm::vec3{scale_factor});
        cascade.world_to_cascade = glm::translate(cascade.world_to_cascade, -rounded_offset);
        cascade.world_to_cascade = bias_mat * cascade.world_to_cascade;

        const auto half_cascade_size = cascade_size / 2.f;
        constexpr auto rsm_pullback_distance = 32.f;
        const auto rsm_view_start = rounded_offset - light.get_direction() * rsm_pullback_distance;
        const auto rsm_view_matrix = glm::lookAt(rsm_view_start, rounded_offset, glm::vec3{ 0, 1, 0 });
        const auto rsm_projection_matrix = glm::ortho(-half_cascade_size, half_cascade_size, -half_cascade_size, half_cascade_size, 0.f, rsm_pullback_distance * 2.f);
        cascade.rsm_vp = rsm_projection_matrix * rsm_view_matrix;
    }
}

void LightPropagationVolume::update_buffers(CommandBuffer& commands) const {
    auto cascade_matrices = std::vector<LPVCascadeMatrices>{};
    cascade_matrices.reserve(cascades.size());
    for (const auto& cascade: cascades) {
        cascade_matrices.push_back(
            {
                .rsm_vp =  cascade.rsm_vp,
                .inverse_rsm_vp = glm::inverse(cascade.rsm_vp),
                .world_to_cascade = cascade.world_to_cascade,
            });
    }

    commands.update_buffer(cascade_data_buffer, cascade_matrices.data(), cascade_matrices.size() * sizeof(LPVCascadeMatrices),
                           0);
}

void LightPropagationVolume::render_rsm(RenderGraph& graph, RenderScene& scene, MeshStorage& meshes) {
    auto cascade_index = 0;
    for (const auto& cascade: cascades) {
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

        graph.add_render_pass(RenderPass{
            .name = "Render RSM and generate VPLs",
            .buffers = {
                {cascade.count_buffer, {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT}},
                {cascade.vpl_buffer,   {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT}},
            },
            .render_pass = rsm_render_pass,
            .render_targets = {
                cascade.flux_target,
                cascade.normals_target,
            },
            .depth_target = cascade.depth_target,
            .clear_values = {
                VkClearValue{.color = {.float32 = {0, 0, 0, 0}}},
                VkClearValue{.color = {.float32 = {0.5, 0.5, 1.0, 0}}},
                VkClearValue{.depthStencil = {.depth = 1.f}}
            },
            .subpasses = {
                {
                    .name = "RSM",
                    .execute = [&](CommandBuffer& commands) {
                        auto global_set = VkDescriptorSet{};
                        backend.create_frame_descriptor_builder()
                               .bind_buffer(0, {
                                   .buffer = cascade_data_buffer
                               }, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                               .bind_buffer(1, {
                                   .buffer = scene.get_primitive_buffer(),
                               }, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                               .build(global_set);

                        commands.bind_descriptor_set(0, global_set);

                        commands.set_push_constant(0, cascade_index);

                        const auto solids = scene.get_solid_primitives();

                        commands.bind_vertex_buffer(0, meshes.get_vertex_position_buffer());
                        commands.bind_vertex_buffer(1, meshes.get_vertex_data_buffer());
                        commands.bind_index_buffer(meshes.get_index_buffer());

                        for (const auto& primitive: solids) {
                            commands.bind_descriptor_set(1, primitive->material->second.descriptor_set);

                            commands.set_push_constant(1, primitive.index);

                            commands.bind_pipeline(primitive->material->first.rsm_pipeline);

                            const auto& mesh = primitive->mesh;
                            commands.draw_indexed(mesh.num_indices, 1, mesh.first_index, mesh.first_vertex, 0);

                            commands.clear_descriptor_set(1);
                        }

                        commands.clear_descriptor_set(0);
                    }
                },
                {
                    .name = "VPL Generation",
                    .execute = [&](CommandBuffer& commands) {
                        const auto sampler = backend.get_default_sampler();

                        const auto set = backend.create_frame_descriptor_builder()
                                                .bind_image(0,
                                                            {.sampler = sampler, .image = cascade.flux_target, .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                                                            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                                                            VK_SHADER_STAGE_FRAGMENT_BIT)
                                                .bind_image(1,
                                                            {.sampler = sampler, .image = cascade.normals_target, .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                                                            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                                                            VK_SHADER_STAGE_FRAGMENT_BIT)
                                                .bind_image(2,
                                                            {.sampler = sampler, .image = cascade.depth_target, .image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL},
                                                            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                                                            VK_SHADER_STAGE_FRAGMENT_BIT)
                                                .bind_buffer(3, {.buffer = cascade_data_buffer},
                                                             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                             VK_SHADER_STAGE_FRAGMENT_BIT)
                                                .bind_buffer(4, {.buffer = cascade.count_buffer},
                                                             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                             VK_SHADER_STAGE_FRAGMENT_BIT)
                                                .bind_buffer(5, {.buffer = cascade.vpl_buffer},
                                                             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                             VK_SHADER_STAGE_FRAGMENT_BIT)
                                                .build();

                        commands.bind_descriptor_set(0, *set);

                        commands.set_push_constant(0, cascade_index);

                        commands.bind_pipeline(vpl_pipeline);

                        commands.draw_triangle();

                        commands.clear_descriptor_set(0);
                    },
                }
            }
        });

        cascade_index++;
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
                                              .build();

                commands.bind_descriptor_set(0, descriptor_set);

                commands.bind_shader(clear_lpv_shader);

                commands.dispatch(4, 32, 32);

                commands.clear_descriptor_set(0);
            }
        }
    );
}

void LightPropagationVolume::inject_lights(RenderGraph& render_graph) const {
    const auto num_cascades = cvar_lpv_num_cascades.Get();

    // For each cascade:
    // - Dispatch a compute shader over the lights. Transform the lights into cascade space, add them to the linked list
    //      of lights for the cell they're in
    // - Dispatch a compute shader over the cascade. Read all the lights from the current cell's light list and add them
    //      to the cascade

    // Build a per-cell linked list of lights
    for (uint32_t cascade_index = 0 ; cascade_index < num_cascades ; cascade_index++) {
        const auto& cascade = cascades[cascade_index];

        render_graph.add_compute_pass(
            {
                .name = fmt::format("Clear light list: cascade {}", cascade_index),
                .buffers = {
                    {cascade.vpl_list_count, {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT}},
                    {cascade.vpl_list_head,  {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT}},
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
                    {cascade.vpl_buffer,     {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT}},
                    {cascade.vpl_list,       {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT |
                                                                                    VK_ACCESS_SHADER_WRITE_BIT}},
                    {cascade.vpl_list_count, {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT |
                                                                                    VK_ACCESS_SHADER_WRITE_BIT}},
                    {cascade.vpl_list_head,  {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT |
                                                                                    VK_ACCESS_SHADER_WRITE_BIT}}
                },
                .execute = [&](CommandBuffer& commands) {
                    auto descriptor_set = *backend.create_frame_descriptor_builder()
                                                  .bind_buffer(
                                                      0, {.buffer = cascade.vpl_buffer},
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
                    lpv_a_red,   {
                                     .stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     .access = VK_ACCESS_SHADER_READ_BIT |
                                               VK_ACCESS_SHADER_WRITE_BIT,
                                     .layout = VK_IMAGE_LAYOUT_GENERAL
                                 },
                },
                {
                    lpv_a_green, {
                                     .stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     .access = VK_ACCESS_SHADER_READ_BIT |
                                               VK_ACCESS_SHADER_WRITE_BIT,
                                     .layout = VK_IMAGE_LAYOUT_GENERAL
                                 },
                },
                {
                    lpv_a_blue,  {
                                     .stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     .access = VK_ACCESS_SHADER_READ_BIT |
                                               VK_ACCESS_SHADER_WRITE_BIT,
                                     .layout = VK_IMAGE_LAYOUT_GENERAL
                                 }
                }
            }
        }
    );

    for (uint32_t cascade_index = 0 ; cascade_index < num_cascades ; cascade_index++) {
        // Walk the linked list and add the lights to the LPV
        const auto& cascade = cascades[cascade_index];

        render_graph.add_compute_pass(
            {
                .name = fmt::format("Inject VPLs into cascade {}", cascade_index),
                .buffers = {
                    {cascade.vpl_buffer,    {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT}},
                    {cascade.vpl_list,      {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT}},
                    {cascade.vpl_list_head, {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT}}
                },
                .execute = [&](CommandBuffer& commands) {
                    auto descriptor_set = *backend.create_frame_descriptor_builder()
                                                  .bind_buffer(
                                                      0, {.buffer = cascade.vpl_buffer},
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
    for (auto step_index = 0 ; step_index < cvar_lpv_num_propagation_steps.Get() ; step_index += 2) {
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
                    lpv_a_red,   {
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
                    lpv_a_blue,  {
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

                for (uint32_t cascade_index = 0 ; cascade_index < cvar_lpv_num_cascades.Get() ; cascade_index
                    ++) {
                    commands.set_push_constant(0, cascade_index);

                    commands.dispatch(1, 32, 32);
                }

                commands.clear_descriptor_set(0);
            }
        }
    );
}

void CascadeData::create_render_targets(ResourceAllocator& allocator) {
    flux_target = allocator.create_texture("RSM Flux", VK_FORMAT_R8G8B8A8_SRGB, glm::uvec2{1024}, 1,
                                           TextureUsage::RenderTarget);
    normals_target = allocator.create_texture("RSM Normals", VK_FORMAT_R8G8B8A8_UNORM, glm::uvec2{1024}, 1,
                                              TextureUsage::RenderTarget);
    depth_target = allocator.create_texture("RSM Depth", VK_FORMAT_D16_UNORM, glm::uvec2{1024}, 1,
                                            TextureUsage::RenderTarget);
}
