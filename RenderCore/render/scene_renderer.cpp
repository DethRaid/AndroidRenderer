#include <spdlog/logger.h>
#include <spdlog/sinks/android_sink.h>

#include "model_import/gltf_model.hpp"
#include "render/scene_renderer.hpp"

#include "antialiasing_type.hpp"
#include "fsr3.hpp"
#include "indirect_drawing_utils.hpp"
#include "backend/blas_build_queue.hpp"
#include "render/backend/render_graph.hpp"
#include "core/system_interface.hpp"
#include "render/render_scene.hpp"
#include "streamline_adapter/streamline_adapter.hpp"

static std::shared_ptr<spdlog::logger> logger;

// ReSharper disable CppDeclaratorNeverUsed
static auto cvar_enable_sun_gi = AutoCVar_Int{
    "r.EnableSunGI", "Whether or not to enable GI from the sun", 1
};

static auto cvar_enable_mesh_lights = AutoCVar_Int{
    "r.MeshLight.Enable", "Whether or not to enable mesh lights", 0
};

static auto cvar_raytrace_mesh_lights = AutoCVar_Int{
    "r.MeshLight.Raytrace", "Whether or not to raytrace mesh lights", 0
};

static auto cvar_use_lpv = AutoCVar_Int{
    "r.lpv.Enable", "Whether to enable the LPV", 1
};

static auto cvar_anti_aliasing = AutoCVar_Enum{
    "r.AntiAliasing", "What kind of antialiasing to use", AntiAliasingType::FSR3
};

static auto cvar_dlss_quality = AutoCVar_Enum{
    "r.DLSS.Quality", "DLSS Quality", sl::DLSSMode::eDLAA
};
// ReSharper restore CppDeclaratorNeverUsed

SceneRenderer::SceneRenderer() {
    logger = SystemInterface::get().get_logger("SceneRenderer");

    // player_view.set_position(glm::vec3{2.f, -1.f, 3.0f});
    player_view.rotate(0, glm::radians(90.f));
    player_view.set_position(glm::vec3{-7.f, 1.f, 0.0f});

    const auto screen_resolution = SystemInterface::get().get_resolution();

    player_view.set_perspective_projection(
        75.f,
        static_cast<float>(screen_resolution.y) /
        static_cast<float>(screen_resolution.x),
        0.05f
    );

    set_output_resolution(screen_resolution);

    auto& backend = RenderBackend::get();

    logger->debug("Initialized render backend");

    if(cvar_use_lpv.Get()) {
        lpv = std::make_unique<LightPropagationVolume>(backend);
        logger->debug("Created LPV");
    }

    if(lpv) {
        lpv->init_resources(backend.get_global_allocator());
        logger->debug("Initialized LPV");
    }

    if(!backend.supports_shading_rate_image) {
        logger->info("Backend does not support VRSAA, turning AA off");
        cvar_anti_aliasing.Set(AntiAliasingType::None);
    }

    logger->info("Initialized SceneRenderer");
}

void SceneRenderer::set_output_resolution(const glm::uvec2& new_output_resolution) {
    output_resolution = new_output_resolution;
}

void SceneRenderer::set_scene(RenderScene& scene_in) {
    scene = &scene_in;
    lighting_pass.set_scene(scene_in);

    auto& backend = RenderBackend::get();

    sun_shadow_drawer = SceneDrawer{
        ScenePassType::Shadow, *scene, meshes, material_storage, backend.get_global_allocator()
    };
    depth_prepass_drawer = SceneDrawer{
        ScenePassType::DepthPrepass, *scene, meshes, material_storage,
        backend.get_global_allocator()
    };
    gbuffer_drawer = SceneDrawer{
        ScenePassType::Gbuffer, *scene, meshes, material_storage, backend.get_global_allocator()
    };

    if(lpv) {
        lpv->set_scene_drawer(
            SceneDrawer{
                ScenePassType::RSM, *scene, meshes, material_storage, backend.get_global_allocator()
            }
        );
    }
}

void SceneRenderer::set_render_resolution(const glm::uvec2 new_render_resolution) {
    if(new_render_resolution == scene_render_resolution) {
        return;
    }

    scene_render_resolution = new_render_resolution;

    logger->info(
        "Setting resolution to {} by {}",
        scene_render_resolution.x,
        scene_render_resolution.y);

    player_view.set_render_resolution(scene_render_resolution);

    player_view.set_aspect_ratio(
        static_cast<float>(scene_render_resolution.x) / static_cast<float>(scene_render_resolution.
            y)
    );

    create_scene_render_targets();
}

void SceneRenderer::render() {
    ZoneScoped;

    auto& backend = RenderBackend::get();

    backend.advance_frame();

    logger->trace("Beginning frame");

    auto* streamline = backend.get_streamline();

    auto needs_motion_vectors = false;

    if(cvar_anti_aliasing.Get() == AntiAliasingType::None) {
        set_render_resolution(output_resolution);
    }

    if(cvar_anti_aliasing.Get() != AntiAliasingType::VRSAA) {
        vrsaa = nullptr;
    } else if(vrsaa == nullptr) {
        vrsaa = std::make_unique<VRSAA>();

        set_render_resolution(output_resolution * 2u);

        vrsaa->init(scene_render_resolution);
    }

    if(streamline) {
        if(cvar_anti_aliasing.Get() == AntiAliasingType::DLSS) {
            streamline->set_dlss_mode(cvar_dlss_quality.Get());

            const auto optimal_render_resolution = streamline->get_dlss_render_resolution(output_resolution);

            set_render_resolution(optimal_render_resolution);
            player_view.set_mip_bias(
                log2(
                    static_cast<double>(optimal_render_resolution.x) / static_cast<double>(output_resolution.x)) - 1.0f
                + FLT_EPSILON);
            needs_motion_vectors = true;

        } else {
            streamline->set_dlss_mode(sl::DLSSMode::eOff);
            player_view.set_mip_bias(0);
        }
    }

    if(cvar_anti_aliasing.Get() == AntiAliasingType::FSR3) {
        if(!fsr3) {
            fsr3 = std::make_unique<FidelityFSSuperResolution3>(backend);
        }

        fsr3->initialize(output_resolution);

        const auto optimal_render_resolution = fsr3->get_optimal_render_resolution();

        set_render_resolution(optimal_render_resolution);
    } else {
        fsr3 = nullptr;
    }

    ui_phase.add_data_upload_passes(backend.get_upload_queue());

    const auto gbuffer_depth_handle = depth_culling_phase.get_depth_buffer();
    lighting_pass.set_gbuffer(
        GBuffer{
            .color = gbuffer_color_handle,
            .normal = gbuffer_normals_handle,
            .data = gbuffer_data_handle,
            .emission = gbuffer_emission_handle,
            .depth = gbuffer_depth_handle,
        }
    );

    backend.get_texture_descriptor_pool().commit_descriptors();

    update_jitter();
    player_view.update_transforms(backend.get_upload_queue());

    if(streamline) {
        streamline->set_constants(player_view, scene_render_resolution);
    }

    auto render_graph = backend.create_render_graph();

    if(last_frame_depth_usage.texture != nullptr) {
        render_graph.set_resource_usage(last_frame_depth_usage, true);
    }
    if(last_frame_normal_usage.texture != nullptr) {
        render_graph.set_resource_usage(last_frame_normal_usage, true);
    }

    render_graph.add_pass(
        {
            .name = "Tracy Collect",
            .execute = [&](const CommandBuffer& commands) {
                backend.collect_tracy_data(commands);
            }
        }
    );

    {
        ZoneScopedN("Begin Frame");
        auto& sun = scene->get_sun_light();
        sun.update_shadow_cascades(player_view);
        sun.update_buffer(backend.get_upload_queue());

        if(lpv) {
            lpv->update_cascade_transforms(player_view, scene->get_sun_light());
            lpv->update_buffers(backend.get_upload_queue());
        }
    }

    backend.get_blas_build_queue().flush_pending_builds(render_graph);

    material_storage.flush_material_buffer(render_graph);

    meshes.flush_mesh_draw_arg_uploads(render_graph);

    meshes.flush_mesh_draw_arg_uploads(render_graph);

    render_graph.add_transition_pass(
        {
            .buffers = {
                {
                    .buffer = scene->get_primitive_buffer(),
                    .stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    .access = VK_ACCESS_SHADER_READ_BIT

                },
                {
                    .buffer = meshes.get_index_buffer(),
                    .stage = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
                    .access = VK_ACCESS_2_INDEX_READ_BIT
                },
                {
                    .buffer = meshes.get_vertex_position_buffer(),
                    .stage = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
                    .access = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT
                },
                {
                    .buffer = meshes.get_vertex_data_buffer(),
                    .stage = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
                    .access = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT
                }
            }
        }
    );

    scene->begin_frame(render_graph);

    scene->generate_emissive_point_clouds(render_graph);

    sky.update_sky_luts(render_graph, scene->get_sun_light().get_direction());

    // Depth and stuff

    depth_culling_phase.render(
        render_graph,
        depth_prepass_drawer,
        material_storage,
        player_view.get_buffer());

    const auto visible_objects_list = depth_culling_phase.get_visible_objects_buffer();
    const auto visible_buffers = translate_visibility_list_to_draw_commands(
        render_graph,
        visible_objects_list,
        scene->get_primitive_buffer(),
        scene->get_total_num_primitives(),
        scene->get_meshes().get_draw_args_buffer());

    if(needs_motion_vectors) {
        motion_vectors_phase.render(
            render_graph,
            depth_prepass_drawer,
            player_view.get_buffer(),
            depth_culling_phase.get_depth_buffer(),
            visible_buffers);
    }

    // LPV

    if(lpv) {
        lpv->clear_volume(render_graph);

        const auto build_mode = LightPropagationVolume::get_build_mode();

        if(*CVarSystem::Get()->GetIntCVar("r.voxel.Enable") != 0 && build_mode == GvBuildMode::Voxels) {
            lpv->build_geometry_volume_from_voxels(render_graph, *scene);
        } else if(build_mode == GvBuildMode::DepthBuffers) {
            lpv->build_geometry_volume_from_scene_view(
                render_graph,
                depth_buffer_mip_chain,
                normal_target_mip_chain,
                player_view.get_buffer(),
                scene_render_resolution / glm::uvec2{2}
            );
        } else if(build_mode == GvBuildMode::PointClouds) {
            lpv->build_geometry_volume_from_point_clouds(render_graph, *scene);
        }

        // VPL cloud generation

        if(cvar_enable_sun_gi.Get()) {
            lpv->inject_indirect_sun_light(render_graph, *scene);
        }

        if(cvar_enable_mesh_lights.Get()) {
            lpv->inject_emissive_point_clouds(render_graph, *scene);
        }
    }

    // Shadows
    // Render shadow pass after RSM so the shadow VS can overlap with the VPL FS
    {
        const auto& sun = scene->get_sun_light();
        sun.render_shadows(render_graph, sun_shadow_drawer);
    }

    if(lpv) {
        lpv->propagate_lighting(render_graph);
    }

    // Generate shading rate image

    /*
     * Use the contrast image from last frame and the depth buffer from last frame to initialize a shading rate image.
     * We'll take more samples in areas of higher contrast and in areas of depth increasing
     *
     * How to handle disocclusions:
     * If the depth increases, something went in front of the current pixel, and we should use the old pixel's contrast.
     * If the depth decreases, the pixel is recently disoccluded, and we should use the maximum sample rate
     *
     * Once we ask for samples, we need to scale them by the available number of samples. Reduce the number of samples,
     * decreasing larger numbers the most, so that we don't take more samples than the hardware supports
     *
     * uint num_samples_4x = num_samples_if_we_take_a_max_of_4_samples_per_pixel()
     * uint num_samples_2x = you get the drill;
     * uint num_samples_1x = yada yada;
     *
     * if(sample_budget <= num_samples_4x) {
     *  write_shading_rate_image(4);
     * } else if(sample_budget <= num_samples_2x_msaa) {
     *  write_shading_rate_image(2);
     * } else {
     *  write_shading_rate_image(1);
     * }
     *
     * Writing the shading rate image allows any sample value below or equal to the max
     */

    std::optional<TextureHandle> vrsaa_shading_rate_image = std::nullopt;
    if(vrsaa) {
        vrsaa->generate_shading_rate_image(render_graph);
        vrsaa_shading_rate_image = vrsaa->get_shading_rate_image();
    }

    // Gbuffers, lighting, and translucency

    gbuffers_phase.render(
        render_graph,
        gbuffer_drawer,
        visible_buffers,
        gbuffer_depth_handle,
        gbuffer_color_handle,
        gbuffer_normals_handle,
        gbuffer_data_handle,
        gbuffer_emission_handle,
        vrsaa_shading_rate_image,
        player_view);

    ao_phase.generate_ao(
        render_graph,
        player_view,
        gbuffer_normals_handle,
        gbuffer_depth_handle,
        ao_handle);

    lighting_pass.render(
        render_graph,
        player_view,
        lit_scene_handle,
        ao_handle,
        lpv.get(),
        sky,
        vrsaa_shading_rate_image);

    // Anti-aliasing/upscaling

    switch(cvar_anti_aliasing.Get()) {
    case AntiAliasingType::None:
        // TODO: Copy lit scene to "antialiased" render target with a bilinear filter
        break;

    case AntiAliasingType::VRSAA:
        if(vrsaa) {
            vrsaa->measure_aliasing(render_graph, gbuffer_color_handle, gbuffer_depth_handle);
            // TODO: Perform a proper VSR resolve, and also do VRS in lighting
        }
        break;

    case AntiAliasingType::FSR3:
        break;

    case AntiAliasingType::DLSS:
        if(streamline) {
            const auto motion_vectors_handle = motion_vectors_phase.get_motion_vectors();
            render_graph.add_pass(
                {
                    .name = "DLSS",
                    .textures = {
                        {
                            .texture = lit_scene_handle, .stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                            .access = VK_ACCESS_2_SHADER_READ_BIT, .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                        },
                        {
                            .texture = antialiased_scene_handle, .stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                            .access = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
                            .layout = VK_IMAGE_LAYOUT_GENERAL
                        },
                        {
                            .texture = gbuffer_depth_handle, .stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                            .access = VK_ACCESS_2_SHADER_READ_BIT, .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                        },
                        {
                            .texture = motion_vectors_handle, .stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                            .access = VK_ACCESS_2_SHADER_READ_BIT, .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                        },
                    },
                    .execute = [&](CommandBuffer& commands) {
                        streamline->evaluate_dlss(
                            commands,
                            lit_scene_handle,
                            antialiased_scene_handle,
                            gbuffer_depth_handle,
                            motion_vectors_handle);
                    }
                });
        }
        break;
    }

    // Bloom

    bloomer.fill_bloom_tex(render_graph, antialiased_scene_handle);

    // Other postprocessing

    // TODO

    // Debug

    if(active_visualization != RenderVisualization::None) {
        draw_debug_visualizers(render_graph);
    }

    // UI

    const auto swapchain_index = backend.get_current_swapchain_index();
    const auto& swapchain_image = swapchain_images.at(swapchain_index);

    render_graph.add_render_pass(
        {
            .name = "UI",
            .textures = {
                {
                    antialiased_scene_handle,
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_READ_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL

                },
                {
                    bloomer.get_bloom_tex(),
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_READ_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL

                }
            },
            .color_attachments = {RenderingAttachmentInfo{.image = swapchain_image}},
            .execute = [&](CommandBuffer& commands) {
                ui_phase.render(commands, player_view, bloomer.get_bloom_tex());
            }
        });

    mip_chain_generator.fill_mip_chain(render_graph, gbuffer_depth_handle, depth_buffer_mip_chain);
    mip_chain_generator.fill_mip_chain(
        render_graph,
        gbuffer_normals_handle,
        normal_target_mip_chain);

    render_graph.add_finish_frame_and_present_pass(
        {
            .swapchain_image = swapchain_image
        }
    );

    render_graph.finish();

    last_frame_depth_usage = render_graph.get_last_usage_token(depth_buffer_mip_chain);
    last_frame_normal_usage = render_graph.get_last_usage_token(normal_target_mip_chain);

    auto& allocator = backend.get_global_allocator();
    allocator.destroy_buffer(visible_buffers.commands);
    allocator.destroy_buffer(visible_buffers.count);
    allocator.destroy_buffer(visible_buffers.primitive_ids);

    backend.execute_graph(std::move(render_graph));
}

SceneView& SceneRenderer::get_local_player() {
    return player_view;
}

TextureLoader& SceneRenderer::get_texture_loader() {
    return texture_loader;
}

MaterialStorage& SceneRenderer::get_material_storage() {
    return material_storage;
}

void SceneRenderer::create_scene_render_targets() {
    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();

    if(gbuffer_color_handle != nullptr) {
        allocator.destroy_texture(gbuffer_color_handle);
    }

    if(gbuffer_normals_handle != nullptr) {
        allocator.destroy_texture(gbuffer_normals_handle);
    }

    if(gbuffer_data_handle != nullptr) {
        allocator.destroy_texture(gbuffer_data_handle);
    }

    if(gbuffer_emission_handle != nullptr) {
        allocator.destroy_texture(gbuffer_emission_handle);
    }

    if(depth_buffer_mip_chain != nullptr) {
        allocator.destroy_texture(depth_buffer_mip_chain);
    }

    if(normal_target_mip_chain != nullptr) {
        allocator.destroy_texture(normal_target_mip_chain);
    }

    if(ao_handle != nullptr) {
        allocator.destroy_texture(ao_handle);
    }

    if(lit_scene_handle != nullptr) {
        allocator.destroy_texture(lit_scene_handle);
    }

    if(antialiased_scene_handle != nullptr) {
        allocator.destroy_texture(antialiased_scene_handle);
    }

    depth_culling_phase.set_render_resolution(scene_render_resolution);

    motion_vectors_phase.set_render_resolution(scene_render_resolution);

    // gbuffer and lighting render targets
    gbuffer_color_handle = allocator.create_texture(
        "gbuffer_color",
        {
            VK_FORMAT_R8G8B8A8_SRGB,
            scene_render_resolution,
            1,
            TextureUsage::RenderTarget
        }
    );

    gbuffer_normals_handle = allocator.create_texture(
        "gbuffer_normals",
        {
            VK_FORMAT_R16G16B16A16_SFLOAT,
            scene_render_resolution,
            1,
            TextureUsage::RenderTarget
        }
    );

    gbuffer_data_handle = allocator.create_texture(
        "gbuffer_data",
        {
            VK_FORMAT_R8G8B8A8_UNORM,
            scene_render_resolution,
            1,
            TextureUsage::RenderTarget
        }
    );

    gbuffer_emission_handle = allocator.create_texture(
        "gbuffer_emission",
        {
            VK_FORMAT_R8G8B8A8_SRGB,
            scene_render_resolution,
            1,
            TextureUsage::RenderTarget
        }
    );

    const auto mip_chain_resolution = scene_render_resolution / glm::uvec2{2};
    const auto minor_dimension = glm::min(mip_chain_resolution.x, mip_chain_resolution.y);
    const auto num_mips = static_cast<uint32_t>(floor(log2(minor_dimension)));
    depth_buffer_mip_chain = allocator.create_texture(
        "Depth buffer mip chain",
        {
            VK_FORMAT_R16_SFLOAT,
            mip_chain_resolution,
            num_mips,
            TextureUsage::StorageImage
        }
    );

    normal_target_mip_chain = allocator.create_texture(
        "gbuffer_normals B",
        {
            VK_FORMAT_R16G16B16A16_SFLOAT,
            mip_chain_resolution,
            num_mips,
            TextureUsage::StorageImage
        }
    );

    ao_handle = allocator.create_texture(
        "AO",
        TextureCreateInfo{
            .format = VK_FORMAT_R32_SFLOAT,
            .resolution = scene_render_resolution,
            .num_mips = 1,
            .usage = TextureUsage::StorageImage,
        }
    );

    lit_scene_handle = allocator.create_texture(
        "lit_scene",
        {
            VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            scene_render_resolution,
            1,
            TextureUsage::RenderTarget
        }
    );

    antialiased_scene_handle = allocator.create_texture(
        "antialiased_scene",
        {
            .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            .resolution = output_resolution,
            .usage = TextureUsage::StorageImage,
            .usage_flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT
        });

    auto& swapchain = backend.get_swapchain();
    const auto& images = swapchain.get_images();
    const auto& image_views = swapchain.get_image_views();
    for(auto swapchain_image_index = 0u; swapchain_image_index < swapchain.image_count;
        swapchain_image_index++) {
        const auto swapchain_image = allocator.emplace_texture(
            GpuTexture{
                .name = fmt::format("Swapchain image {}", swapchain_image_index),
                .create_info = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                    .imageType = VK_IMAGE_TYPE_2D,
                    .format = swapchain.image_format,
                    .extent = VkExtent3D{swapchain.extent.width, swapchain.extent.height, 1},
                    .mipLevels = 1,
                    .arrayLayers = 1,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .tiling = VK_IMAGE_TILING_OPTIMAL,
                    .usage = swapchain.image_usage_flags,
                },
                .image = images->at(swapchain_image_index),
                .image_view = image_views->at(swapchain_image_index),
                .type = TextureAllocationType::Swapchain,
            }
        );

        swapchain_images.push_back(swapchain_image);
    }

    ui_phase.set_resources(
        antialiased_scene_handle,
        glm::uvec2{swapchain.extent.width, swapchain.extent.height});
}

void SceneRenderer::update_jitter() {
    previous_jitter = jitter;
    jitter = glm::vec2{jitter_sequence_x.get_next_value(), jitter_sequence_y.get_next_value()} - 0.5f;
    jitter /= glm::vec2{scene_render_resolution};

    player_view.set_jitter(jitter);
}

void SceneRenderer::draw_debug_visualizers(RenderGraph& render_graph) {
    switch(active_visualization) {
    case RenderVisualization::None:
        // Intentionally empty
        break;

    case RenderVisualization::VoxelizedMeshes:
        if(*CVarSystem::Get()->GetIntCVar("r.voxel.Enable") != 0 &&
            *CVarSystem::Get()->GetIntCVar("r.voxel.Visualize") != 0) {
            voxel_visualizer.render(
                render_graph,
                *scene,
                lit_scene_handle,
                player_view.get_buffer());
        }
        break;

    case RenderVisualization::VPLs:
        lpv->visualize_vpls(
            render_graph,
            player_view.get_buffer(),
            lit_scene_handle,
            depth_culling_phase.get_depth_buffer());
        break;
    }
}

MeshStorage& SceneRenderer::get_mesh_storage() {
    return meshes;
}

void SceneRenderer::translate_player(const glm::vec3& movement) {
    player_view.translate(movement);
}

void SceneRenderer::rotate_player(const float delta_pitch, const float delta_yaw) {
    player_view.rotate(delta_pitch, delta_yaw);
}

void SceneRenderer::set_imgui_commands(ImDrawData* im_draw_data) {
    ui_phase.set_imgui_draw_data(im_draw_data);
}

void SceneRenderer::set_active_visualizer(const RenderVisualization visualizer) {
    active_visualization = visualizer;
}
