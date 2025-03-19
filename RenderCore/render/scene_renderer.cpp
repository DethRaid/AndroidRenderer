#include <spdlog/logger.h>
#include <spdlog/sinks/android_sink.h>

#include "model_import/gltf_model.hpp"
#include "render/scene_renderer.hpp"
#include "render/antialiasing_type.hpp"
#include "render/indirect_drawing_utils.hpp"
#include "render/backend/blas_build_queue.hpp"
#include "render/backend/render_graph.hpp"
#include "core/system_interface.hpp"
#include "render/render_scene.hpp"
#include "render/upscaling/fsr3.hpp"
#include "render/upscaling/dlss.hpp"
#include "render/upscaling/xess.hpp"

static std::shared_ptr<spdlog::logger> logger;

enum class GIMode {
    Off,
    LPV,
    RT
};

// ReSharper disable CppDeclaratorNeverUsed
static auto cvar_gi_mode = AutoCVar_Enum{
    "r.GI.Mode", "How to calculate GI", GIMode::LPV
};

static auto cvar_enable_mesh_lights = AutoCVar_Int{
    "r.MeshLight.Enable", "Whether or not to enable mesh lights", 1
};

static auto cvar_raytrace_mesh_lights = AutoCVar_Int{
    "r.MeshLight.Raytrace", "Whether or not to raytrace mesh lights", 0
};

static auto cvar_anti_aliasing = AutoCVar_Enum{
    "r.AntiAliasing", "What kind of antialiasing to use", AntiAliasingType::DLSS
};

/*
 * Quick guide to anti-aliasing quality modes:
 *
 * Name               | DLSS   | XeSS | FSR  
 * ---------------------------------- -------
 * Anti-Aliasing      | 1.0x   | 1.0x | 1.0x 
 * Ultra Quality Plus | N/A    | 1.3x | N/A  
 * Ultra Quality      | N/A    | 1.5x | N/A  
 * Quality            | 1.5x   | 1.7x | 1.5x 
 * Balanced           | 1.724x | 2.0x | 1.7x 
 * Performance        | 2.0x   | 2.3x | 2.0x 
 * Ultra Performance  | 3.0x   | 3.0x | 3.0x 
 */

// ReSharper restore CppDeclaratorNeverUsed

SceneRenderer::SceneRenderer() {
    logger = SystemInterface::get().get_logger("SceneRenderer");

    // player_view.set_position(glm::vec3{2.f, -1.f, 3.0f});
    player_view.rotate(0, glm::radians(90.f));
    player_view.set_position(glm::vec3{-7.f, 1.f, 0.0f});

    const auto screen_resolution = SystemInterface::get().get_resolution();

    set_output_resolution(screen_resolution);

    auto& backend = RenderBackend::get();

    logger->debug("Initialized render backend");

    if(!backend.supports_shading_rate_image && cvar_anti_aliasing.Get() == AntiAliasingType::VRSAA) {
        logger->info("Backend does not support VRSAA, turning AA off");
        cvar_anti_aliasing.Set(AntiAliasingType::None);
    }

    copy_scene_pso = backend.begin_building_pipeline("Copy Scene")
                            .set_vertex_shader("shaders/common/fullscreen.vert.spv")
                            .set_fragment_shader("shaders/util/copy_with_sampler.frag.spv")
                            .set_depth_state({.enable_depth_test = false, .enable_depth_write = false})
                            .build();
    linear_sampler = backend.get_global_allocator().get_sampler(
        {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
        });

    stbn_3d_unitvec = NoiseTexture::create("assets/stbn/stbn_unitvec3_2Dx1D_128x128x64", 64, texture_loader);

    logger->info("Initialized SceneRenderer");
}

void SceneRenderer::set_output_resolution(const glm::uvec2& new_output_resolution) {
    output_resolution = new_output_resolution;
}

void SceneRenderer::set_scene(RenderScene& scene_in) {
    scene = &scene_in;
    lighting_pass.set_scene(scene_in);
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

    player_view.set_perspective_projection(
        75.f,
        static_cast<float>(scene_render_resolution.x) /
        static_cast<float>(scene_render_resolution.y),
        0.05f
    );

    create_scene_render_targets();
}

void SceneRenderer::render() {
    ZoneScoped;

    auto& backend = RenderBackend::get();

    backend.advance_frame();

    logger->trace("Beginning frame");

    auto needs_motion_vectors = false;

    switch(cvar_anti_aliasing.Get()) {
    case AntiAliasingType::None:
        vrsaa = nullptr;
        upscaler = nullptr;
        set_render_resolution(output_resolution / glm::uvec2{2});
        player_view.set_mip_bias(0);
        break;

    case AntiAliasingType::VRSAA:
        if(vrsaa == nullptr) {
            vrsaa = std::make_unique<VRSAA>();
        }

        upscaler = nullptr;

        set_render_resolution(output_resolution * 2u);

        vrsaa->init(scene_render_resolution);
        player_view.set_mip_bias(0);

        break;

    case AntiAliasingType::DLSS:
#if SAH_USE_STREAMLINE
        if(cached_aa != AntiAliasingType::DLSS) {
            upscaler = std::make_unique<DLSSAdapter>();
        }
#endif
        break;
    case AntiAliasingType::FSR3:
#if SAH_USE_FFX
        if(cached_aa != AntiAliasingType::FSR3) {
            upscaler = std::make_unique<FidelityFSSuperResolution3>();
        }
#endif
        break;
    case AntiAliasingType::XeSS:
#if SAH_USE_XESS
        if(cached_aa != AntiAliasingType::XeSS) {
            upscaler = std::make_unique<XeSSAdapter>();
        }
#endif
        break;
    }

    cached_aa = cvar_anti_aliasing.Get();

    if(upscaler) {
        vrsaa = nullptr;

        upscaler->initialize(output_resolution, frame_count);

        const auto optimal_render_resolution = upscaler->get_optimal_render_resolution();
        set_render_resolution(optimal_render_resolution);

        const auto d_output_resolution = glm::vec2{output_resolution};
        const auto d_render_resolution = glm::vec2{optimal_render_resolution};
        player_view.set_mip_bias(log2(d_render_resolution.x / d_output_resolution.x) - 1.0f);

        needs_motion_vectors = true;
    }

    if(cvar_gi_mode.Get() == GIMode::LPV) {
        if(lpv == nullptr) {
            lpv = std::make_unique<LightPropagationVolume>();
        }
    } else {
        lpv = nullptr;
    }

    if(cvar_gi_mode.Get() == GIMode::RT) {
        if(rtgi == nullptr) {
            rtgi = std::make_unique<RayTracedGlobalIllumination>();
        }
    } else {
        rtgi = nullptr;
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

    if(upscaler) {
        upscaler->set_constants(player_view, scene_render_resolution);
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
        if(DirectionalLight::get_shadow_mode() == SunShadowMode::CascadedShadowMaps) {
            sun.update_shadow_cascades(player_view);
        }

        sun.update_buffer(backend.get_upload_queue());

        if(lpv) {
            lpv->update_cascade_transforms(player_view, scene->get_sun_light());
            lpv->update_buffers(backend.get_upload_queue());
        }
    }

    backend.get_blas_build_queue().flush_pending_builds(render_graph);

    material_storage.flush_material_instance_buffer(render_graph);

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
        *scene,
        material_storage,
        player_view.get_buffer());

    const auto visible_objects_list = depth_culling_phase.get_visible_objects_buffer();
    const auto visible_solids_buffers = translate_visibility_list_to_draw_commands(
        render_graph,
        visible_objects_list,
        scene->get_primitive_buffer(),
        scene->get_total_num_primitives(),
        scene->get_meshes().get_draw_args_buffer(),
        PRIMITIVE_TYPE_SOLID);
    const auto visible_masked_buffers = translate_visibility_list_to_draw_commands(
        render_graph,
        visible_objects_list,
        scene->get_primitive_buffer(),
        scene->get_total_num_primitives(),
        scene->get_meshes().get_draw_args_buffer(),
        PRIMITIVE_TYPE_CUTOUT);

    if(needs_motion_vectors) {
        motion_vectors_phase.render(
            render_graph,
            *scene,
            player_view.get_buffer(),
            depth_culling_phase.get_depth_buffer(),
            visible_solids_buffers,
            visible_masked_buffers);
    }

    // LPV

    if(lpv) {
        lpv->clear_volume(render_graph);

        const auto build_mode = LightPropagationVolume::get_build_mode();
        if(build_mode == GvBuildMode::DepthBuffers) {
            lpv->build_geometry_volume_from_scene_view(
                render_graph,
                depth_buffer_mip_chain,
                normal_target_mip_chain,
                player_view.get_buffer(),
                scene_render_resolution / glm::uvec2{2}
            );
        }

        // VPL cloud generation

        lpv->inject_indirect_sun_light(render_graph, *scene);

        if(cvar_enable_mesh_lights.Get()) {
            lpv->inject_emissive_point_clouds(render_graph, *scene);
        }
    }

    // Shadows
    // Render shadow pass after RSM so the shadow VS can overlap with the VPL FS
    if(DirectionalLight::get_shadow_mode() == SunShadowMode::CascadedShadowMaps) {
        const auto& sun = scene->get_sun_light();
        sun.render_shadows(render_graph, *scene);
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

    gbuffer_phase.render(
        render_graph,
        *scene,
        visible_solids_buffers,
        visible_masked_buffers,
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
        *scene,
        stbn_3d_unitvec,
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
        vrsaa_shading_rate_image,
        stbn_3d_unitvec);

    //rt_debug_phase.raytrace(render_graph, player_view, *scene, gbuffer_phase);

    // Anti-aliasing/upscaling

    evaluate_antialiasing(render_graph, gbuffer_depth_handle);

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
    allocator.destroy_buffer(visible_solids_buffers.commands);
    allocator.destroy_buffer(visible_solids_buffers.count);
    allocator.destroy_buffer(visible_solids_buffers.primitive_ids);
    allocator.destroy_buffer(visible_masked_buffers.commands);
    allocator.destroy_buffer(visible_masked_buffers.count);
    allocator.destroy_buffer(visible_masked_buffers.primitive_ids);

    backend.execute_graph(render_graph);

    frame_count++;
}

void SceneRenderer::evaluate_antialiasing(RenderGraph& render_graph, const TextureHandle gbuffer_depth_handle) const {
    auto& backend = RenderBackend::get();

    switch(cvar_anti_aliasing.Get()) {
    case AntiAliasingType::VRSAA:
        if(vrsaa) {
            vrsaa->measure_aliasing(render_graph, gbuffer_color_handle, gbuffer_depth_handle);
            // TODO: Perform a proper VSR resolve, and also do VRS in lighting
        }
        break;

    case AntiAliasingType::FSR3:
        [[fallthrough]];
    case AntiAliasingType::DLSS:
        [[fallthrough]];
    case AntiAliasingType::XeSS:
        if(upscaler) {
            const auto motion_vectors_handle = motion_vectors_phase.get_motion_vectors();
            upscaler->evaluate(
                render_graph,
                lit_scene_handle,
                antialiased_scene_handle,
                gbuffer_depth_handle,
                motion_vectors_handle);
            break;
        } else {
            [[fallthrough]];
        }

    case AntiAliasingType::None: {
        const auto set = backend.get_transient_descriptor_allocator()
                                .build_set(copy_scene_pso, 0)
                                .bind(lit_scene_handle, linear_sampler)
                                .build();
        render_graph.add_render_pass(
            {
                .name = "Copy scene",
                .descriptor_sets = {set},
                .color_attachments = {
                    {
                        .image = antialiased_scene_handle,
                        .load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE
                    }
                },
                .execute = [&](CommandBuffer& commands) {
                    commands.set_push_constant(0, 1.f / static_cast<float>(output_resolution.x));
                    commands.set_push_constant(1, 1.f / static_cast<float>(output_resolution.y));
                    commands.bind_descriptor_set(0, set);
                    commands.bind_pipeline(copy_scene_pso);
                    commands.draw_triangle();

                    commands.clear_descriptor_set(0);
                }
            });
    }
    }
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

    motion_vectors_phase.set_render_resolution(scene_render_resolution, output_resolution);

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
            .usage = TextureUsage::RenderTarget,
            .usage_flags = VK_IMAGE_USAGE_STORAGE_BIT
        }
    );

    lit_scene_handle = allocator.create_texture(
        "lit_scene",
        {
            //VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            .format = VK_FORMAT_R16G16B16A16_SFLOAT,
            .resolution = scene_render_resolution,
            .usage = TextureUsage::RenderTarget,
            .usage_flags = VK_IMAGE_USAGE_STORAGE_BIT
        }
    );

    antialiased_scene_handle = allocator.create_texture(
        "antialiased_scene",
        {
            .format = VK_FORMAT_R16G16B16A16_SFLOAT, //VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            .resolution = output_resolution,
            .usage = TextureUsage::RenderTarget,
            .usage_flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT
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

    if(upscaler) {
        jitter = upscaler->get_jitter();
    }

    player_view.set_jitter(jitter);
}

void SceneRenderer::draw_debug_visualizers(RenderGraph& render_graph) {
    switch(active_visualization) {
    case RenderVisualization::None:
        // Intentionally empty
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
