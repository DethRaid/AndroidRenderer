#include "light_propagation_volume.hpp"

#include <magic_enum.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "backend/pipeline_builder.hpp"
#include "backend/pipeline_cache.hpp"
#include "backend/render_graph.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/resource_allocator.hpp"
#include "console/cvars.hpp"
#include "core/system_interface.hpp"
#include "render/scene_view.hpp"
#include "render/render_scene.hpp"
#include "sdf/voxel_cache.hpp"
#include "shared/vpl.hpp"
#include "shared/lpv.hpp"
#include "shared/view_info.hpp"

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
    "Number of times to propagate lighting through the LPV", 16
};

static auto cvar_lpv_behind_camera_percent = AutoCVar_Float{
    "r.LPV.PercentBehindCamera",
    "The percentage of the LPV that should be behind the camera. Not exact",
    0.1
};

static auto cvar_lpv_build_gv_mode = AutoCVar_Enum{
    "r.LPV.GvBuildMode",
    "How to build the geometry volume.\n0 = Disable\n1 = Use the RSM depth buffer and last frame's depth buffer\n2 = Use voxels from the renderer's voxel cache",
    GvBuildMode::DepthBuffers
};

static auto cvar_lpv_rsm_resolution = AutoCVar_Int{
    "r.lpv.RsmResolution",
    "Resolution for the RSM targets",
    512
};

static auto cvar_lpv_use_compute_vpl_injection = AutoCVar_Int{
    "r.LPV.ComputeVPL",
    "Whether to use a compute pipeline or a raster pipeline to inject VPLs into the LPVs",
    0
};

static std::shared_ptr<spdlog::logger> logger;

LightPropagationVolume::LightPropagationVolume(RenderBackend& backend_in) : backend{backend_in} {
    if(logger == nullptr) {
        logger = SystemInterface::get().get_logger("LightPropagationVolume");
    }

    auto& pipeline_cache = backend.get_pipeline_cache();
    clear_lpv_shader = pipeline_cache.create_pipeline("shaders/lpv/clear_lpv.comp.spv");

    inject_voxels_into_gv_shader = pipeline_cache.create_pipeline("shaders/lpv/inject_voxels_into_gv.comp.spv");

    propagation_shader = pipeline_cache.create_pipeline("shaders/lpv/lpv_propagate.comp.spv");

    linear_sampler = backend.get_global_allocator().get_sampler(
        {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = 16,
            .maxLod = 16,
            .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK
        }
    );

    vpl_pipeline = pipeline_cache.create_pipeline("shaders/lpv/rsm_generate_vpls.comp.spv");

    if(cvar_lpv_use_compute_vpl_injection.Get() == 0) {
        vpl_injection_pipeline = backend.begin_building_pipeline("VPL Injection")
                                        .set_topology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
                                        .set_vertex_shader("shaders/lpv/vpl_injection.vert.spv")
                                        .set_fragment_shader("shaders/lpv/vpl_injection.frag.spv")
                                        .set_blend_state(
                                            0,
                                            {
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
                                            1,
                                            {
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
                                            2,
                                            {
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
    } else {
        vpl_injection_compute_pipeline = pipeline_cache.create_pipeline("shaders/lpv/vpl_injection.comp.spv");
    }

    inject_rsm_depth_into_gv_pipeline = backend.begin_building_pipeline("GV Injection")
                                               .set_topology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
                                               .set_vertex_shader("shaders/lpv/gv_injection.vert.spv")
                                               .set_fragment_shader("shaders/lpv/gv_injection.frag.spv")
                                               .set_depth_state(
                                                   {.enable_depth_test = VK_FALSE, .enable_depth_write = VK_FALSE}
                                               )
                                               .set_blend_state(
                                                   0,
                                                   {
                                                       .blendEnable = VK_TRUE,
                                                       .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                                       .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                                       .colorBlendOp = VK_BLEND_OP_MAX,
                                                       .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                       .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                       .alphaBlendOp = VK_BLEND_OP_MAX,
                                                       .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                       VK_COLOR_COMPONENT_G_BIT |
                                                       VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                                                   }
                                               )
                                               .build();
    inject_scene_depth_into_gv_pipeline = backend.begin_building_pipeline("Inject scene depth into GV")
                                                 .set_topology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
                                                 .set_vertex_shader("shaders/lpv/inject_scene_depth_into_gv.vert.spv")
                                                 .set_geometry_shader("shaders/lpv/inject_scene_depth_into_gv.geom.spv")
                                                 .set_fragment_shader("shaders/lpv/inject_scene_depth_into_gv.frag.spv")
                                                 .set_depth_state(
                                                     {.enable_depth_test = VK_FALSE, .enable_depth_write = VK_FALSE}
                                                 )
                                                 .set_blend_state(
                                                     0,
                                                     {
                                                         .blendEnable = VK_TRUE,
                                                         .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                                         .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                                         .colorBlendOp = VK_BLEND_OP_MAX,
                                                         .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                         .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                         .alphaBlendOp = VK_BLEND_OP_MAX,
                                                         .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                         VK_COLOR_COMPONENT_G_BIT |
                                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                                                     }
                                                 )
                                                 .build();

    lpv_render_shader = backend.begin_building_pipeline("LPV Rendering")
                               .set_vertex_shader("shaders/common/fullscreen.vert.spv")
                               .set_fragment_shader("shaders/lpv/overlay.frag.spv")
                               .set_depth_state({.enable_depth_test = VK_FALSE, .enable_depth_write = VK_FALSE})
                               .set_blend_state(
                                   0,
                                   {
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
        "LPV Red A",
        VK_FORMAT_R16G16B16A16_SFLOAT,
        texture_resolution,
        1,
        TextureUsage::StorageImage
    );
    lpv_a_green = allocator.create_volume_texture(
        "LPV Green A",
        VK_FORMAT_R16G16B16A16_SFLOAT,
        texture_resolution,
        1,
        TextureUsage::StorageImage
    );
    lpv_a_blue = allocator.create_volume_texture(
        "LPV Blue A",
        VK_FORMAT_R16G16B16A16_SFLOAT,
        texture_resolution,
        1,
        TextureUsage::StorageImage
    );
    lpv_b_red = allocator.create_volume_texture(
        "LPV Red B",
        VK_FORMAT_R16G16B16A16_SFLOAT,
        texture_resolution,
        1,
        TextureUsage::StorageImage
    );
    lpv_b_green = allocator.create_volume_texture(
        "LPV Green B",
        VK_FORMAT_R16G16B16A16_SFLOAT,
        texture_resolution,
        1,
        TextureUsage::StorageImage
    );
    lpv_b_blue = allocator.create_volume_texture(
        "LPV Blue B",
        VK_FORMAT_R16G16B16A16_SFLOAT,
        texture_resolution,
        1,
        TextureUsage::StorageImage
    );

    geometry_volume_handle = allocator.create_volume_texture(
        "Geometry Volume",
        VK_FORMAT_R16G16B16A16_SFLOAT,
        texture_resolution,
        1,
        TextureUsage::StorageImage
    );

    cascade_data_buffer = allocator.create_buffer(
        "LPV Cascade Data",
        sizeof(LPVCascadeMatrices) * num_cascades,
        BufferUsage::UniformBuffer
    );

    cascades.resize(num_cascades);
    uint32_t cascade_index = 0;
    for(auto& cascade : cascades) {
        cascade.create_render_targets(allocator);
        cascade.count_buffer = allocator.create_buffer(
            fmt::format("Cascade {} VPL Count", cascade_index),
            sizeof(VkDrawIndirectCommand),
            BufferUsage::IndirectBuffer
        );
        cascade.vpl_buffer = allocator.create_buffer(
            fmt::format("Cascade {} VPL List", cascade_index),
            sizeof(PackedVPL) * 65536,
            BufferUsage::StorageBuffer
        );
        cascade_index++;
    }
}

void LightPropagationVolume::set_scene_drawer(SceneDrawer&& drawer) {
    rsm_drawer = drawer;
}

void LightPropagationVolume::update_cascade_transforms(const SceneTransform& view, const SunLight& light) {
    const auto num_cells = cvar_lpv_resolution.Get();
    const auto base_cell_size = cvar_lpv_cell_size.GetFloat();

    const auto& view_position = view.get_position();

    // Position the LPV slightly in front of the view. We want some of the LPV to be behind it for reflections and such

    const auto num_cascades = cvar_lpv_num_cascades.Get();

    const auto offset_distance_scale = 0.5f - cvar_lpv_behind_camera_percent.GetFloat();

    for(int32_t cascade_index = 0; cascade_index < num_cascades; cascade_index++) {
        const auto cell_size = base_cell_size * static_cast<float>(glm::pow(2.f, cascade_index));
        const auto cascade_size = static_cast<float>(num_cells) * cell_size;

        // Offset the center point of the cascade by 20% of the length of one side
        // When the camera is aligned with the X or Y axis, this will offset the cascade by 20% of its length. 30% of it
        // will be behind the camera, 70% of it will be in front. This feels reasonable
        // When the camera is 45 degrees off of the X or Y axis, the cascade will have more of itself behind the camera
        // This might be fine

        const auto offset_distance = cascade_size * offset_distance_scale;
        const auto offset = view_position + view.get_forward() * offset_distance;

        // Round to the cell size to prevent flickering
        const auto snapped_offset = glm::round(offset / glm::vec3{cell_size * 2.f}) * cell_size * 2.f;

        const auto scale_factor = 1.0f / cascade_size;

        constexpr auto bias_mat = glm::mat4{
            0.5f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.5f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.5f, 0.0f,
            0.5f, 0.5f, 0.5f, 1.0f
        };

        auto& cascade = cascades[cascade_index];
        cascade.world_to_cascade = glm::mat4{1.f};
        cascade.world_to_cascade = glm::scale(cascade.world_to_cascade, glm::vec3{scale_factor});
        cascade.world_to_cascade = glm::translate(cascade.world_to_cascade, -snapped_offset);
        cascade.world_to_cascade = bias_mat * cascade.world_to_cascade;

        const auto half_cascade_size = cascade_size / 2.f;
        constexpr auto rsm_pullback_distance = 32.f;
        const auto rsm_view_start = snapped_offset - light.get_direction() * rsm_pullback_distance;
        const auto rsm_view_matrix = glm::lookAt(rsm_view_start, snapped_offset, glm::vec3{0, 1, 0});
        const auto rsm_projection_matrix = glm::ortho(
            -half_cascade_size,
            half_cascade_size,
            -half_cascade_size,
            half_cascade_size,
            0.f,
            rsm_pullback_distance * 2.f
        );
        cascade.rsm_vp = rsm_projection_matrix * rsm_view_matrix;

        cascade.min_bounds = snapped_offset - glm::vec3{half_cascade_size};
        cascade.max_bounds = snapped_offset + glm::vec3{half_cascade_size};
    }
}

void LightPropagationVolume::update_buffers(CommandBuffer& commands) const {
    auto cascade_matrices = std::vector<LPVCascadeMatrices>{};
    cascade_matrices.reserve(cascades.size());
    for(const auto& cascade : cascades) {
        cascade_matrices.emplace_back(
            LPVCascadeMatrices{
                .rsm_vp = cascade.rsm_vp,
                .inverse_rsm_vp = glm::inverse(cascade.rsm_vp),
                .world_to_cascade = cascade.world_to_cascade,
            }
        );
    }

    commands.update_buffer(
        cascade_data_buffer,
        cascade_matrices.data(),
        static_cast<uint32_t>(cascade_matrices.size() * sizeof(LPVCascadeMatrices)),
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
    for(const auto& cascade : cascades) {
        graph.add_pass(
            {
                .name = "Clear count buffer",
                .buffers = {
                    {
                        cascade.count_buffer,
                        {.stage = VK_PIPELINE_STAGE_TRANSFER_BIT, .access = VK_ACCESS_TRANSFER_WRITE_BIT}
                    }
                },
                .execute = [&](CommandBuffer& commands) {
                    commands.fill_buffer(cascade.count_buffer, 0);
                }
            }
        );

        graph.add_render_pass(
            DynamicRenderingPass{
                .name = "Render RSM",
                .buffers = {
                    {
                        cascade_data_buffer,
                        {
                            .stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                            .access = VK_ACCESS_UNIFORM_READ_BIT
                        }
                    }
                },
                .color_attachments = {
                    {
                        .image = cascade.flux_target,
                        .load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .store_op = VK_ATTACHMENT_STORE_OP_STORE,
                        .clear_value = {.color = {.float32 = {0, 0, 0, 0}}}
                    },
                    {
                        .image = cascade.normals_target,
                        .load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .store_op = VK_ATTACHMENT_STORE_OP_STORE,
                        .clear_value = {.color = {.float32 = {0.5, 0.5, 1.0, 0}}}
                    }
                },
                .depth_attachment = RenderingAttachmentInfo{
                    .image = cascade.depth_target,
                    .load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .store_op = VK_ATTACHMENT_STORE_OP_STORE,
                    .clear_value = {.depthStencil = {.depth = 1.f}}
                },
                .execute = [&](CommandBuffer& commands) {
                    auto global_set = *vkutil::DescriptorBuilder::begin(
                                           backend,
                                           backend.get_transient_descriptor_allocator()
                                       )
                                       .bind_buffer(
                                           0,
                                           {.buffer = cascade_data_buffer},
                                           VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                           VK_SHADER_STAGE_VERTEX_BIT
                                       )
                                       .bind_buffer(
                                           1,
                                           {.buffer = scene.get_sun_light().get_constant_buffer()},
                                           VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                           VK_SHADER_STAGE_FRAGMENT_BIT
                                       )
                                       .build();

                    commands.bind_descriptor_set(0, global_set);

                    commands.set_push_constant(4, cascade_index);

                    rsm_drawer.draw(commands);

                    commands.clear_descriptor_set(0);
                }
            });

        {
            auto descriptor_set = backend.get_transient_descriptor_allocator()
                                         .build_set(vpl_pipeline, 0)
                                         .bind(0, cascade.flux_target, backend.get_default_sampler())
                                         .bind(1, cascade.normals_target, backend.get_default_sampler())
                                         .bind(2, cascade.depth_target, backend.get_default_sampler())
                                         .bind(3, cascade_data_buffer)
                                         .finalize();

            struct VplPipelineConstants {
                DeviceAddress count_buffer_address;
                DeviceAddress vpl_buffer_address;
                uint32_t cascade_index;

                VplPipelineConstants(
                    const ResourceAllocator& allocator, const BufferHandle count_buffer, const BufferHandle vpl_buffer,
                    const uint32_t cascade_index
                ) : cascade_index{cascade_index} {
                    const auto& count_buffer_actual = allocator.get_buffer(count_buffer);
                    count_buffer_address = count_buffer_actual.address;

                    const auto& vpl_buffer_actual = allocator.get_buffer(vpl_buffer);
                    vpl_buffer_address = vpl_buffer_actual.address;
                }
            };

            const auto resolution = glm::uvec2{static_cast<uint32_t>(cvar_lpv_rsm_resolution.Get())};

            graph.add_compute_dispatch<VplPipelineConstants>(
                ComputeDispatch<VplPipelineConstants>{
                    .name = "Extract VPLs",
                    .descriptor_sets = {descriptor_set},
                    .push_constants = VplPipelineConstants{
                        backend.get_global_allocator(), cascade.count_buffer, cascade.vpl_buffer, cascade_index
                    },
                    .num_workgroups = {resolution, 1},
                    .compute_shader = vpl_pipeline
                });
        }

        dispatch_vpl_injection_pass(graph, cascade_index, cascade);

        /*
         *        graph.begin_render_pass(
            {
                .name = "Render RSM and generate VPLs",
                .buffers = {
                    {
                        cascade.count_buffer,
                        {
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                            VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT
                        }
                    },
                    {
                        cascade.vpl_buffer,
                        {
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                            VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT
                        }
                    },
                    {
                        cascade_data_buffer,
                        {
                            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                            VK_ACCESS_UNIFORM_READ_BIT
                        }
                    }
                },
                .attachments = {
                    cascade.flux_target,
                    cascade.normals_target,
                    cascade.depth_target
                },
                .clear_values = {
                    VkClearValue{.color = {.float32 = {0, 0, 0, 0}}},
                    VkClearValue{.color = {.float32 = {0.5, 0.5, 1.0, 0}}},
                    VkClearValue{.depthStencil = {.depth = 1.f}},
                },
            }
        );
        graph.add_subpass(
            Subpass{
                .name = "RSM",
                .color_attachments = {0, 1},
                .depth_attachment = 2,
                .execute = [&](CommandBuffer& commands) {
                    auto global_set = *vkutil::DescriptorBuilder::begin(
                                           backend,
                                           backend.get_transient_descriptor_allocator()
                                       )
                                       .bind_buffer(
                                           0,
                                           {.buffer = cascade_data_buffer},
                                           VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                           VK_SHADER_STAGE_VERTEX_BIT
                                       )
                                       .bind_buffer(
                                           1,
                                           {.buffer = scene.get_sun_light().get_constant_buffer()},
                                           VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                           VK_SHADER_STAGE_FRAGMENT_BIT
                                       )
                                       .build();

                    commands.bind_descriptor_set(0, global_set);

                    commands.set_push_constant(4, cascade_index);

                    rsm_drawer.draw(commands);

                    commands.clear_descriptor_set(0);
                }
            }
        );
        graph.add_subpass(
            {
                .name = "VPL Generation",
                .input_attachments = {0, 1, 2},
                .execute = [&](CommandBuffer& commands) {
                    const auto sampler = backend.get_default_sampler();

                    const auto set = vkutil::DescriptorBuilder::begin(
                                         backend,
                                         backend.get_transient_descriptor_allocator()
                                     )
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
                                         3,
                                         {.buffer = cascade_data_buffer},
                                         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                         VK_SHADER_STAGE_FRAGMENT_BIT
                                     )
                                     .build();

                    commands.bind_descriptor_set(0, *set);

                    commands.bind_buffer_reference(0, cascade.count_buffer);
                    commands.bind_buffer_reference(2, cascade.vpl_buffer);

                    commands.set_push_constant(4, cascade_index);

                    commands.bind_pipeline(vpl_pipeline);

                    commands.draw_triangle();

                    commands.clear_descriptor_set(0);
                },
            }
        );
        graph.end_render_pass();


        graph.begin_render_pass(
            {
                .name = "VPL Injection",
                .buffers = {
                    {cascade_data_buffer, {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT}},
                    {cascade.vpl_buffer, {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT}},
                    {cascade.count_buffer, {VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT}},
                },
                .attachments = {lpv_a_red, lpv_a_green, lpv_a_blue}
            }
        );
        graph.add_subpass(
            {
                .name = "VPL Injection",
                .color_attachments = {0, 1, 2},
                .execute = [&](CommandBuffer& commands) {
                    const auto set = *vkutil::DescriptorBuilder::begin(
                                          backend,
                                          backend.get_transient_descriptor_allocator()
                                      )
                                      .bind_buffer(
                                          0,
                                          {.buffer = cascade_data_buffer},
                                          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                          VK_SHADER_STAGE_VERTEX_BIT
                                      )
                                      .build();

                    commands.bind_descriptor_set(0, set);

                    commands.bind_buffer_reference(0, cascade.vpl_buffer);
                    commands.set_push_constant(2, cascade_index);
                    commands.set_push_constant(3, static_cast<uint32_t>(cvar_lpv_num_cascades.Get()));

                    commands.bind_pipeline(vpl_injection_pipeline);

                    commands.draw_indirect(cascade.count_buffer);

                    commands.clear_descriptor_set(0);
                }
            }
        );
        graph.end_render_pass();
        */

        if(cvar_lpv_build_gv_mode.Get() == GvBuildMode::DepthBuffers) {
            inject_rsm_depth_into_cascade_gv(graph, cascade, cascade_index);
        }

        cascade_index++;
    }

    graph.end_label();
}

void LightPropagationVolume::dispatch_vpl_injection_pass(
    RenderGraph& graph, const uint32_t cascade_index, const CascadeData& cascade
) {
    auto descriptor_set = backend.get_transient_descriptor_allocator()
                                 .build_set(vpl_injection_pipeline, 0)
                                 .bind(0, cascade_data_buffer)
                                 .finalize();

    if(cvar_lpv_use_compute_vpl_injection.Get() == 0) {
        graph.add_render_pass(
            DynamicRenderingPass{
                .name = "VPL Injection",
                .buffers = {
                    {cascade_data_buffer, {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT}},
                    {cascade.vpl_buffer, {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT}},
                    {
                        cascade.count_buffer,
                        {VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT}
                    },
                },
                .descriptor_sets = std::vector{descriptor_set},
                .color_attachments = {
                    RenderingAttachmentInfo{
                        .image = lpv_a_red,
                        .load_op = VK_ATTACHMENT_LOAD_OP_LOAD,
                        .store_op = VK_ATTACHMENT_STORE_OP_STORE
                    },
                    RenderingAttachmentInfo{
                        .image = lpv_a_green,
                        .load_op = VK_ATTACHMENT_LOAD_OP_LOAD,
                        .store_op = VK_ATTACHMENT_STORE_OP_STORE
                    },
                    RenderingAttachmentInfo{
                        .image = lpv_a_blue,
                        .load_op = VK_ATTACHMENT_LOAD_OP_LOAD,
                        .store_op = VK_ATTACHMENT_STORE_OP_STORE
                    },
                },
                .view_mask = 0,
                .execute = [=, this](CommandBuffer& commands) {
                    commands.bind_descriptor_set(0, descriptor_set);

                    commands.bind_buffer_reference(0, cascade.vpl_buffer);
                    commands.set_push_constant(2, cascade_index);
                    commands.set_push_constant(3, static_cast<uint32_t>(cvar_lpv_num_cascades.Get()));

                    commands.bind_pipeline(vpl_injection_pipeline);

                    commands.draw_indirect(cascade.count_buffer);

                    commands.clear_descriptor_set(0);
                }
            });
    } else {
        struct VplInjectionConstants {
            DeviceAddress vpl_buffer_address;
            uint32_t cascade_index;
            uint32_t num_cascades;

            VplInjectionConstants(
                const ResourceAllocator& allocator, const BufferHandle vpl_buffer, const uint32_t cascade_index, const uint32_t num_cascades
            ) : cascade_index{cascade_index}, num_cascades{num_cascades} {
                const auto& count_buffer_actual = allocator.get_buffer(vpl_buffer);
                vpl_buffer_address = count_buffer_actual.address;
            }
        };

        graph.add_compute_dispatch<VplInjectionConstants>(
            IndirectComputeDispatch< VplInjectionConstants>{
                .name = "VPL Injection",
                .descriptor_sets = std::vector{descriptor_set},
                .buffers = {
                    {cascade_data_buffer, {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT}},
                    {cascade.vpl_buffer, {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT}},
                    {
                        cascade.count_buffer,
                        {VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT}
                    },
                },
                .push_constants = VplInjectionConstants{
                    backend.get_global_allocator(),
                    cascade.vpl_buffer,
                    cascade_index,
                    static_cast<uint32_t>(cvar_lpv_num_cascades.Get())
                },
                .dispatch = cascade.count_buffer,
                .compute_shader = vpl_injection_compute_pipeline
            }
        );
    }
}


void LightPropagationVolume::inject_emissive_point_clouds(RenderGraph& graph, const RenderScene& scene) {
    graph.begin_label("Emissive mesh injection");

    for(auto cascade_index = 0u; cascade_index < cvar_lpv_num_cascades.Get(); cascade_index++) {
        const auto& cascade = cascades[cascade_index];

        const auto& primitives = scene.get_primitives_in_bounds(cascade.min_bounds, cascade.max_bounds);
        if(primitives.empty()) {
            continue;
        }

        graph.begin_render_pass(
            {
                .name = "VPL Injection",
                .buffers = {
                    {cascade_data_buffer, {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT}},
                    {cascade.vpl_buffer, {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT}},
                    {cascade.count_buffer, {VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT}},
                },
                .attachments = {lpv_a_red, lpv_a_green, lpv_a_blue},
            }
        );
        graph.add_subpass(
            {
                .name = "VPL Injection",
                .color_attachments = {0, 1, 2},
                .execute = [&](CommandBuffer& commands) {
                    const auto set = *vkutil::DescriptorBuilder::begin(
                                          backend,
                                          backend.get_transient_descriptor_allocator()
                                      )
                                      .bind_buffer(
                                          0,
                                          {.buffer = cascade_data_buffer},
                                          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                          VK_SHADER_STAGE_VERTEX_BIT
                                      )
                                      .build();

                    commands.bind_descriptor_set(0, set);

                    commands.set_push_constant(2, cascade_index);
                    commands.set_push_constant(3, static_cast<uint32_t>(cvar_lpv_num_cascades.Get()));

                    commands.bind_pipeline(vpl_injection_pipeline);

                    for(const auto& primitive : primitives) {
                        if(!primitive->material->first.emissive) {
                            continue;
                        }

                        commands.bind_buffer_reference(0, primitive->emissive_points_buffer);
                        commands.draw(primitive->mesh->num_points);
                    }

                    commands.clear_descriptor_set(0);
                }
            }
        );
        graph.end_render_pass();
    }

    graph.end_label();
}

void LightPropagationVolume::clear_volume(RenderGraph& render_graph) {
    render_graph.add_pass(
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
                auto descriptor_set = *vkutil::DescriptorBuilder::begin(
                                           backend,
                                           backend.get_transient_descriptor_allocator()
                                       )
                                       .bind_image(
                                           0,
                                           {
                                               .image = lpv_a_red,
                                               .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                           },
                                           VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                           VK_SHADER_STAGE_COMPUTE_BIT
                                       )
                                       .bind_image(
                                           1,
                                           {
                                               .image = lpv_a_green,
                                               .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                           },
                                           VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                           VK_SHADER_STAGE_COMPUTE_BIT
                                       )
                                       .bind_image(
                                           2,
                                           {
                                               .image = lpv_a_blue,
                                               .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                           },
                                           VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                           VK_SHADER_STAGE_COMPUTE_BIT
                                       )
                                       .bind_image(
                                           3,
                                           {
                                               .image = geometry_volume_handle,
                                               .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                           },
                                           VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                           VK_SHADER_STAGE_COMPUTE_BIT
                                       )
                                       .build();

                commands.bind_descriptor_set(0, descriptor_set);

                commands.bind_pipeline(clear_lpv_shader);

                commands.dispatch(cvar_lpv_num_cascades.Get(), 32, 32);

                commands.clear_descriptor_set(0);
            }
        }
    );
}

GvBuildMode LightPropagationVolume::get_build_mode() {
    return cvar_lpv_build_gv_mode.Get();
}

void LightPropagationVolume::build_geometry_volume_from_voxels(
    RenderGraph& render_graph, const RenderScene& scene
) {
    /*
     * For each cascade:
     * - Get the static mesh primitives in the cascade
     * - Dispatch a compute shader to add them to the GV
     */

    auto& allocator = backend.get_global_allocator();
    auto& voxel_cache = scene.get_voxel_cache();

    const auto num_cascades = cvar_lpv_num_cascades.Get();
    for(auto cascade_idx = 0u; cascade_idx < num_cascades; cascade_idx++) {
        const auto& cascade = cascades[cascade_idx];
        const auto& primitives = scene.get_primitives_in_bounds(cascade.min_bounds, cascade.max_bounds);

        if(primitives.empty()) {
            continue;
        }

        // Two arrays: one for the voxels for each primitive, one that maps from thread ID to primitive ID
        auto primitive_ids = std::vector<uint32_t>{};
        auto textures = std::vector<vkutil::DescriptorBuilder::ImageInfo>{};
        primitive_ids.reserve(primitives.size());
        textures.reserve(primitives.size());

        for(const auto& primitive : primitives) {
            primitive_ids.push_back(primitive.index);
            const auto& mesh = primitive->mesh;
            const auto& voxel = voxel_cache.get_voxel_for_primitive(primitive);
            textures.emplace_back(
                vkutil::DescriptorBuilder::ImageInfo{
                    .sampler = backend.get_default_sampler(),
                    .image = voxel.voxels_color,
                    .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                }
            );
        }

        const auto primitive_id_buffer = allocator.create_buffer(
            "Primitive ID buffer",
            primitive_ids.size() * sizeof(uint32_t),
            BufferUsage::StagingBuffer
        );

        render_graph.add_pass(
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
                    {cascade_data_buffer, {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_UNIFORM_READ_BIT}},
                    {primitive_id_buffer, {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT}}
                },
                .execute = [&, primitive_ids = std::move(primitive_ids), textures = std::move(textures),
                    primitive_id_buffer = primitive_id_buffer](
                CommandBuffer& commands
            ) {
                    commands.update_buffer(
                        primitive_id_buffer,
                        primitive_ids.data(),
                        primitive_ids.size() * sizeof(uint32_t),
                        0
                    );

                    const auto set = *vkutil::DescriptorBuilder::begin(
                                          backend,
                                          backend.get_transient_descriptor_allocator()
                                      )
                                      .bind_buffer(
                                          0,
                                          {.buffer = cascade_data_buffer},
                                          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                          VK_SHADER_STAGE_COMPUTE_BIT
                                      )
                                      .bind_buffer(
                                          1,
                                          {.buffer = primitive_id_buffer},
                                          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                          VK_SHADER_STAGE_COMPUTE_BIT
                                      )
                                      .bind_buffer(
                                          2,
                                          {.buffer = scene.get_primitive_buffer()},
                                          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                          VK_SHADER_STAGE_COMPUTE_BIT
                                      )
                                      .bind_image(
                                          3,
                                          {
                                              .image = geometry_volume_handle,
                                              .image_layout = VK_IMAGE_LAYOUT_GENERAL
                                          },
                                          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                          VK_SHADER_STAGE_COMPUTE_BIT
                                      )
                                      .bind_image_array(
                                          4,
                                          textures,
                                          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                          VK_SHADER_STAGE_COMPUTE_BIT
                                      )
                                      .build();

                    commands.bind_descriptor_set(0, set);
                    commands.set_push_constant(0, static_cast<uint32_t>(textures.size()));
                    commands.set_push_constant(1, cascade_idx);
                    commands.bind_pipeline(inject_voxels_into_gv_shader);

                    commands.dispatch(32, 32, 32);

                    commands.clear_descriptor_set(0);
                }
            }
        );

        allocator.destroy_buffer(primitive_id_buffer);
    }
}

void LightPropagationVolume::build_geometry_volume_from_scene_view(
    RenderGraph& graph, const TextureHandle depth_buffer, const TextureHandle normal_target,
    const BufferHandle view_uniform_buffer, const glm::uvec2 resolution
) {
    graph.begin_render_pass(
        {
            .name = "Inject RSM depth into GV",
            .textures = {
                {
                    depth_buffer,
                    {
                        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    }
                },
                {
                    normal_target,
                    {
                        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    }
                }
            },
            .attachments = {geometry_volume_handle},
        }
    );
    graph.add_subpass(
        {
            .name = "Inject RSM depth into GV",
            .color_attachments = {0},
            .execute = [&](CommandBuffer& commands) {
                const auto effective_resolution = resolution / glm::uvec2{2};

                const auto sampler = backend.get_default_sampler();
                const auto set = *vkutil::DescriptorBuilder::begin(
                                      backend,
                                      backend.get_transient_descriptor_allocator()
                                  )
                                  .bind_image(
                                      0,
                                      {
                                          .sampler = sampler, .image = normal_target,
                                          .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                      },
                                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      VK_SHADER_STAGE_VERTEX_BIT
                                  )
                                  .bind_image(
                                      1,
                                      {
                                          .sampler = sampler, .image = depth_buffer,
                                          .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                      },
                                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      VK_SHADER_STAGE_VERTEX_BIT
                                  )
                                  .bind_buffer(
                                      2,
                                      {.buffer = cascade_data_buffer},
                                      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                      VK_SHADER_STAGE_GEOMETRY_BIT
                                  )
                                  .bind_buffer(
                                      3,
                                      {.buffer = view_uniform_buffer},
                                      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                      VK_SHADER_STAGE_VERTEX_BIT
                                  )
                                  .build();

                commands.bind_descriptor_set(0, set);

                commands.set_push_constant(0, effective_resolution.x);
                commands.set_push_constant(1, effective_resolution.y);
                commands.set_push_constant(2, static_cast<uint32_t>(cvar_lpv_num_cascades.Get()));

                commands.bind_pipeline(inject_scene_depth_into_gv_pipeline);

                commands.draw(effective_resolution.x * effective_resolution.y / 4);

                commands.clear_descriptor_set(0);
            }
        }
    );
    graph.end_render_pass();
}

void LightPropagationVolume::build_geometry_volume_from_point_clouds(
    RenderGraph& render_graph, const RenderScene& scene
) {
    // /*
    //    * For each cascade:
    //    * - Get the static mesh primitives in the cascade
    //    * - Dispatch a compute shader to add them to the GV
    //    */
    // 
    // auto& allocator = backend.get_global_allocator();
    // 
    // const auto num_cascades = cvar_lpv_num_cascades.Get();
    // for (auto cascade_idx = 0u; cascade_idx < num_cascades; cascade_idx++) {
    //     const auto& cascade = cascades[cascade_idx];
    //     const auto& primitives = scene.get_primitives_in_bounds(cascade.min_bounds, cascade.max_bounds);
    // 
    //     if (primitives.empty()) {
    //         continue;
    //     }
    // 
    //     auto buffer_tokens = std::vector<BufferUsageToken>{};
    //     buffer_tokens.reserve(primitives.size() + 1);
    // 
    // 
    //     // Two arrays: one for the voxels for each primitive, one that maps from thread ID to primitive ID
    //     auto primitive_ids = std::vector<uint32_t>{};
    //     auto textures = std::vector<vkutil::DescriptorBuilder::ImageInfo>{};
    //     primitive_ids.reserve(primitives.size());
    //     textures.reserve(primitives.size());
    // 
    //     for (const auto& primitive : primitives) {
    //         primitive_ids.push_back(primitive.index);
    //         const auto& mesh = primitive->mesh;
    //         mesh->
    //         const auto& voxel = voxel_cache.get_voxel_for_mesh(mesh);
    //         textures.emplace_back(
    //             vkutil::DescriptorBuilder::ImageInfo{
    //                 .sampler = backend.get_default_sampler(),
    //                 .image = voxel.voxels,
    //                 .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    //             }
    //         );
    //     }
    // 
    //     const auto primitive_id_buffer = allocator.create_buffer(
    //         "Primitive ID buffer", primitive_ids.size() * sizeof(uint32_t), BufferUsage::StagingBuffer
    //     );
    // 
    //     render_graph.add_compute_pass(
    //         {
    //             .name = "Voxel injection",
    //             .textures = {
    //                 {
    //                     geometry_volume_handle,
    //                     {
    //                         VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
    //                         VK_IMAGE_LAYOUT_GENERAL
    //                     },
    //                 }
    //             },
    //             .buffers = {
    //                 {cascade_data_buffer, {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_UNIFORM_READ_BIT}},
    //                 {primitive_id_buffer, {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT}}
    //             },
    //             .execute = [&, primitive_ids = std::move(primitive_ids), textures = std::move(textures),
    //                 primitive_id_buffer = primitive_id_buffer](
    //             CommandBuffer& commands
    //         ) {
    //                 commands.update_buffer(
    //                     primitive_id_buffer, primitive_ids.data(), primitive_ids.size() * sizeof(uint32_t), 0
    //                 );
    // 
    //                 const auto set = *backend.create_frame_descriptor_builder()
    //                                          .bind_buffer(
    //                                              0, {.buffer = cascade_data_buffer}, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    //                                              VK_SHADER_STAGE_COMPUTE_BIT
    //                                          )
    //                                          .bind_buffer(
    //                                              1, {.buffer = primitive_id_buffer}, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    //                                              VK_SHADER_STAGE_COMPUTE_BIT
    //                                          )
    //                                          .bind_buffer(
    //                                              2, {.buffer = scene.get_primitive_buffer()},
    //                                              VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT
    //                                          )
    //                                          .bind_image(
    //                                              3, {
    //                                                  .image = geometry_volume_handle,
    //                                                  .image_layout = VK_IMAGE_LAYOUT_GENERAL
    //                                              }, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT
    //                                          )
    //                                          .bind_image_array(
    //                                              4, textures, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    //                                              VK_SHADER_STAGE_COMPUTE_BIT
    //                                          )
    //                                          .build();
    // 
    //                 commands.bind_descriptor_set(0, set);
    //                 commands.set_push_constant(0, static_cast<uint32_t>(textures.size()));
    //                 commands.set_push_constant(1, cascade_idx);
    //                 commands.bind_shader(inject_voxels_into_gv_shader);
    // 
    //                 commands.dispatch(32, 32, 32);
    // 
    //                 commands.clear_descriptor_set(0);
    //             }
    //         }
    //     );
    // 
    //     allocator.destroy_buffer(primitive_id_buffer);
    // }
}

void LightPropagationVolume::propagate_lighting(RenderGraph& render_graph) {
    render_graph.begin_label("LPV Propagation");

    bool use_gv = false;

    for(auto step_index = 0; step_index < cvar_lpv_num_propagation_steps.Get(); step_index += 2) {
        perform_propagation_step(
            render_graph,
            lpv_a_red,
            lpv_a_green,
            lpv_a_blue,
            lpv_b_red,
            lpv_b_green,
            lpv_b_blue,
            use_gv
        );
        use_gv = true;
        perform_propagation_step(
            render_graph,
            lpv_b_red,
            lpv_b_green,
            lpv_b_blue,
            lpv_a_red,
            lpv_a_green,
            lpv_a_blue,
            use_gv
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
    CommandBuffer& commands, const VkDescriptorSet gbuffers_descriptor, const BufferHandle scene_view_buffer
) const {
    GpuZoneScoped(commands)

    commands.begin_label("LightPropagationVolume::add_lighting_to_scene");

    commands.bind_descriptor_set(0, gbuffers_descriptor);

    auto lpv_descriptor = *vkutil::DescriptorBuilder::begin(
                               backend,
                               backend.get_transient_descriptor_allocator()
                           )
                           .bind_image(
                               0,
                               {
                                   .sampler = linear_sampler, .image = lpv_a_red,
                                   .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                               },
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_FRAGMENT_BIT
                           )
                           .bind_image(
                               1,
                               {
                                   .sampler = linear_sampler, .image = lpv_a_green,
                                   .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                               },
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_FRAGMENT_BIT
                           )
                           .bind_image(
                               2,
                               {
                                   .sampler = linear_sampler, .image = lpv_a_blue,
                                   .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                               },
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_FRAGMENT_BIT
                           )
                           .bind_buffer(
                               3,
                               {.buffer = cascade_data_buffer},
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                               VK_SHADER_STAGE_FRAGMENT_BIT
                           )
                           .bind_buffer(
                               4,
                               {.buffer = scene_view_buffer},
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                               VK_SHADER_STAGE_FRAGMENT_BIT
                           )
                           .build();
    commands.bind_descriptor_set(1, lpv_descriptor);

    commands.bind_pipeline(lpv_render_shader);

    commands.set_push_constant(0, static_cast<uint32_t>(cvar_lpv_num_cascades.Get()));

    commands.draw_triangle();

    commands.clear_descriptor_set(1);
    commands.clear_descriptor_set(0);

    commands.end_label();
}

void LightPropagationVolume::inject_rsm_depth_into_cascade_gv(
    RenderGraph& graph, const CascadeData& cascade, const uint32_t cascade_index
) {
    auto& allocator = backend.get_global_allocator();

    auto view_matrices = allocator.create_buffer(
        "GV View Matrices Buffer",
        sizeof(ViewInfo),
        BufferUsage::UniformBuffer
    );

    // We just need this data
    const auto view = ViewInfo{
        .inverse_view = glm::inverse(cascade.rsm_vp),
        .inverse_projection = glm::mat4{1.f},
    };

    graph.add_pass(
        ComputePass{
            .name = "Update view buffer",
            .execute = [&](CommandBuffer& commands) {
                commands.update_buffer(view_matrices, view);
            }
        }
    );

    const auto rsm_resolution = glm::uvec2{static_cast<uint32_t>(cvar_lpv_rsm_resolution.Get())};

    graph.begin_render_pass(
        {
            .name = "Inject RSM depth into GV",
            .textures = {
                {
                    cascade.depth_target,
                    {
                        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    }
                },
                {
                    cascade.normals_target,
                    {
                        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    }
                }
            },
            .attachments = {geometry_volume_handle}
        }
    );
    graph.add_subpass(
        {
            .name = "Inject RSM depth into GV",
            .color_attachments = {0},
            .execute = [&](CommandBuffer& commands) {
                const auto sampler = backend.get_default_sampler();
                const auto set = *vkutil::DescriptorBuilder::begin(
                                      backend,
                                      backend.get_transient_descriptor_allocator()
                                  )
                                  .bind_image(
                                      0,
                                      {
                                          .sampler = sampler, .image = cascade.normals_target,
                                          .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                      },
                                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      VK_SHADER_STAGE_VERTEX_BIT
                                  )
                                  .bind_image(
                                      1,
                                      {
                                          .sampler = sampler, .image = cascade.depth_target,
                                          .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                      },
                                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      VK_SHADER_STAGE_VERTEX_BIT
                                  )
                                  .bind_buffer(
                                      2,
                                      {.buffer = cascade_data_buffer},
                                      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                      VK_SHADER_STAGE_VERTEX_BIT
                                  )
                                  .bind_buffer(
                                      3,
                                      {.buffer = view_matrices},
                                      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                      VK_SHADER_STAGE_VERTEX_BIT
                                  )
                                  .build();

                commands.bind_descriptor_set(0, set);

                commands.set_push_constant(0, cascade_index);
                commands.set_push_constant(1, rsm_resolution.x);
                commands.set_push_constant(2, rsm_resolution.y);

                commands.bind_pipeline(inject_rsm_depth_into_gv_pipeline);

                commands.draw(rsm_resolution.x * rsm_resolution.y);

                commands.clear_descriptor_set(0);
            }
        }
    );
    graph.end_render_pass();

    allocator.destroy_buffer(view_matrices);
}

void LightPropagationVolume::perform_propagation_step(
    RenderGraph& render_graph,
    const TextureHandle read_red, const TextureHandle read_green, const TextureHandle read_blue,
    const TextureHandle write_red, const TextureHandle write_green, const TextureHandle write_blue,
    const bool use_gv
) const {
    render_graph.add_pass(
        {
            .name = "Propagate lighting",
            .textures = {
                {
                    read_red, {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL}
                },
                {
                    read_green,
                    {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL}
                },
                {
                    read_blue,
                    {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL}
                },
                {
                    write_red,
                    {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL}
                },
                {
                    write_green,
                    {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL}
                },
                {
                    write_blue,
                    {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL}
                },
                {
                    geometry_volume_handle,
                    {
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    }
                },
            },
            .execute = [&](CommandBuffer& commands) {
                const auto descriptor_set =
                    *vkutil::DescriptorBuilder::begin(
                         backend,
                         backend.get_transient_descriptor_allocator()
                     )
                     .bind_image(
                         0,
                         {.image = read_red, .image_layout = VK_IMAGE_LAYOUT_GENERAL},
                         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                         VK_SHADER_STAGE_COMPUTE_BIT
                     )
                     .bind_image(
                         1,
                         {.image = read_green, .image_layout = VK_IMAGE_LAYOUT_GENERAL},
                         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                         VK_SHADER_STAGE_COMPUTE_BIT
                     )
                     .bind_image(
                         2,
                         {.image = read_blue, .image_layout = VK_IMAGE_LAYOUT_GENERAL},
                         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                         VK_SHADER_STAGE_COMPUTE_BIT
                     )
                     .bind_image(
                         3,
                         {.image = write_red, .image_layout = VK_IMAGE_LAYOUT_GENERAL},
                         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                         VK_SHADER_STAGE_COMPUTE_BIT
                     )
                     .bind_image(
                         4,
                         {.image = write_green, .image_layout = VK_IMAGE_LAYOUT_GENERAL},
                         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                         VK_SHADER_STAGE_COMPUTE_BIT
                     )
                     .bind_image(
                         5,
                         {.image = write_blue, .image_layout = VK_IMAGE_LAYOUT_GENERAL},
                         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                         VK_SHADER_STAGE_COMPUTE_BIT
                     )
                     .bind_image(
                         6,
                         {
                             .sampler = linear_sampler, .image = geometry_volume_handle,
                             .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                         },
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                         VK_SHADER_STAGE_COMPUTE_BIT
                     )
                     .build();

                {
                    commands.bind_descriptor_set(0, descriptor_set);

                    commands.bind_pipeline(propagation_shader);

                    commands.set_push_constant(1, use_gv ? 1u : 0u);

                    for(uint32_t cascade_index = 0; cascade_index < cvar_lpv_num_cascades.Get(); cascade_index++) {
                        commands.set_push_constant(0, cascade_index);

                        commands.dispatch(1, 32, 32);
                    }

                    commands.clear_descriptor_set(0);
                }
            }
        }
    );
}

void CascadeData::create_render_targets(ResourceAllocator& allocator) {
    const auto resolution = glm::uvec2{static_cast<uint32_t>(cvar_lpv_rsm_resolution.Get())};
    flux_target = allocator.create_texture(
        "RSM Flux",
        VK_FORMAT_R8G8B8A8_SRGB,
        resolution,
        1,
        TextureUsage::RenderTarget
    );
    normals_target = allocator.create_texture(
        "RSM Normals",
        VK_FORMAT_R8G8B8A8_UNORM,
        resolution,
        1,
        TextureUsage::RenderTarget
    );
    depth_target = allocator.create_texture(
        "RSM Depth",
        VK_FORMAT_D16_UNORM,
        resolution,
        1,
        TextureUsage::RenderTarget
    );
}
