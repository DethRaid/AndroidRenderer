#include <spdlog/logger.h>
#include <spdlog/sinks/android_sink.h>

#include "gltf/gltf_model.hpp"
#include "render/scene_renderer.hpp"
#include "render/backend/render_graph.hpp"
#include "core/system_interface.hpp"
#include "render/render_scene.hpp"

static std::shared_ptr<spdlog::logger> logger;

static auto cvar_num_shadow_cascades = AutoCVar_Int{"r.Shadow.NumCascades", "Number of shadow cascades", 4};

static auto cvar_shadow_cascade_resolution = AutoCVar_Int{
    "r.Shadow.CascadeResolution",
    "Resolution of one cascade in the shadowmap", 1024
};

static auto cvar_max_shadow_distance = AutoCVar_Float{"r.Shadow.Distance", "Maximum distance of shadows", 128};

static auto cvar_shadow_cascade_split_lambda = AutoCVar_Float{
    "r.Shadow.CascadeSplitLambda",
    "Factor to use when calculating shadow cascade splits", 0.95
};

SceneRenderer::SceneRenderer() :
    backend{}, player_view{backend}, texture_loader{backend}, material_storage{backend},
    meshes{backend.get_global_allocator(), backend.get_upload_queue()}, mip_chain_generator{backend}, lpv{backend},
    lighting_pass{backend},
    ui_phase{*this} {
    logger = SystemInterface::get().get_logger("SceneRenderer");

    // player_view.set_position_and_direction(glm::vec3{10.f, 0.f, 20.0f}, glm::vec3{0.f, 0.0f, -1.f});
    player_view.set_position_and_direction(glm::vec3{ 7.f, 1.f, 0.0f }, glm::vec3{ -1.0f, 0.0f, 0.f });

    const auto render_resolution = SystemInterface::get().get_resolution();

    player_view.set_perspective_projection(
        75.f, static_cast<float>(render_resolution.y) /
        static_cast<float>(render_resolution.x), 0.05f
    );

    create_shadow_render_targets();

    set_render_resolution(render_resolution);

    if (*CVarSystem::Get()->GetIntCVar("r.voxel.Enable") != 0) {
        voxel_cache = std::make_unique<VoxelCache>(backend);
    }

    lpv.init_resources(backend.get_global_allocator());

    logger->info("Initialized SceneRenderer");
}

void SceneRenderer::set_render_resolution(const glm::uvec2& resolution) {
    ZoneScoped;

    if (resolution == scene_render_resolution) {
        return;
    }

    logger->info("Setting resolution to {} by {}", resolution.x, resolution.y);

    scene_render_resolution = resolution;

    player_view.set_render_resolution(scene_render_resolution);

    player_view.set_aspect_ratio(
        static_cast<float>(scene_render_resolution.x) / static_cast<float>(scene_render_resolution.y)
    );

    create_scene_render_targets();
}

void SceneRenderer::set_scene(RenderScene& scene_in) {
    scene = &scene_in;
    lighting_pass.set_scene(scene_in);

    sun_shadow_drawer = SceneDrawer{ScenePassType::Shadow, *scene, meshes, material_storage};
    gbuffer_drawer = SceneDrawer{ScenePassType::Gbuffer, *scene, meshes, material_storage};

    lpv.set_scene_drawer(SceneDrawer{ScenePassType::RSM, *scene, meshes, material_storage});
}

void SceneRenderer::render() {
    ZoneScoped;

    backend.advance_frame();

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

    auto render_graph = backend.create_render_graph();

    render_graph.set_resource_usage(depth_buffer_mip_chain, last_frame_depth_usage, true);
    render_graph.set_resource_usage(normal_target_mip_chain, last_frame_normal_usage, true);

    render_graph.add_compute_pass(
        {
            .name = "Tracy Collect",
            .execute = [&](const CommandBuffer& commands) { backend.collect_tracy_data(commands); }
        }
    );

    render_graph.add_compute_pass(
        ComputePass{
            .name = "Begin Frame",
            .execute = [&](CommandBuffer& commands) {
                GpuZoneScopedN(commands, "Begin Frame")

                auto& sun = scene->get_sun_light();
                sun.update_shadow_cascades(player_view);
                sun.update_buffer(commands);

                player_view.update_transforms(commands);

                lpv.update_cascade_transforms(player_view, scene->get_sun_light());
                lpv.update_buffers(commands);
            }
        }
    );

    material_storage.flush_material_buffer(render_graph);

    scene->flush_primitive_upload(render_graph);

    lpv.clear_volume(render_graph);

    render_graph.add_transition_pass(
        {
            .buffers = {
                {
                    scene->get_primitive_buffer(),
                    {
                        .stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        .access = VK_ACCESS_SHADER_READ_BIT
                    }
                }
            }
        }
    );

    if (voxel_cache && lpv.get_build_mode() == GvBuildMode::Voxels) {
        lpv.build_geometry_volume_from_voxels(render_graph, *scene, *voxel_cache);
    } else if (lpv.get_build_mode() == GvBuildMode::DepthBuffers) {
        lpv.build_geometry_volume_from_scene_view(
            render_graph, depth_buffer_mip_chain, normal_target_mip_chain, player_view.get_buffer(),
            scene_render_resolution / glm::uvec2{ 2 }
        );
    }

    // VPL cloud generation

    lpv.inject_indirect_sun_light(render_graph, *scene);

    // Shadows
    // Render shadow pass after RSM so the shadow VS can overlap with the VPL FS
    render_graph.add_render_pass(
        RenderPass{
            .name = "CSM sun shadow",
            .attachments = {shadowmap_handle},
            .clear_values = std::vector{
                VkClearValue{.depthStencil = {.depth = 1.f}}
            },
            .view_mask = 1u | 2u | 4u | 8u,
            .subpasses = {
                Subpass{
                    .name = "Sun shadow",
                    .depth_attachment = {0},
                    .execute = [&](CommandBuffer& commands) {
                        GpuZoneScopedN(commands, "Sun Shadow")

                        auto& sun = scene->get_sun_light();

                        auto global_set = *backend.create_frame_descriptor_builder()
                                                  .bind_buffer(
                                                      0, {
                                                          .buffer = sun.get_constant_buffer()
                                                      }, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT
                                                  )
                                                  .build();

                        commands.bind_descriptor_set(0, global_set);

                        sun_shadow_drawer.draw(commands);

                        commands.clear_descriptor_set(0);
                    }
                }
            }
        }
    );

    lpv.propagate_lighting(render_graph);

    // Gbuffers, lighting, and translucency

    render_graph.add_render_pass(
        RenderPass{
            .name = "Scene pass",
            .textures = {
                {
                    shadowmap_handle,
                    {
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    }
                }
            },
            .attachments = std::vector{
                gbuffer_color_handle,
                gbuffer_normals_handle,
                gbuffer_data_handle,
                gbuffer_emission_handle,
                lit_scene_handle,
                gbuffer_depth_handle,
            },
            .clear_values = std::vector{
                // Clear color targets to black, clear depth to 1.f
                VkClearValue{.color = {.float32 = {0, 0, 0, 0}}},
                VkClearValue{.color = {.float32 = {0.5f, 0.5f, 1.f, 0}}},
                VkClearValue{.color = {.float32 = {0, 0, 0, 0}}},
                VkClearValue{.color = {.float32 = {0, 0, 0, 0}}},
                VkClearValue{.color = {.float32 = {0, 0, 0, 0}}},

                VkClearValue{.depthStencil = {.depth = 1.f}},
            },
            .subpasses = {
                Subpass{
                    .name = "Gbuffer",
                    .color_attachments = {0, 1, 2, 3},
                    .depth_attachment = {5},
                    .execute = [&](CommandBuffer& commands) {
                        GpuZoneScopedN(commands, "GBuffer")

                        auto global_set = *backend.create_frame_descriptor_builder()
                                                  .bind_buffer(
                                                      0, {
                                                          .buffer = player_view.get_buffer()
                                                      }, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT
                                                  )
                                                  .build();

                        commands.bind_descriptor_set(0, global_set);

                        gbuffer_drawer.draw(commands);

                        commands.clear_descriptor_set(0);
                    }
                },
                Subpass{
                    .name = "Lighting",
                    .input_attachments = {0, 1, 2, 3, 5},
                    .color_attachments = {4},
                    .execute = [&](CommandBuffer& commands) {
                        lighting_pass.render(commands, player_view, lpv);
                    }
                },
                // TODO
                // Subpass{
                //     .name = "Translucency",
                //     .color_attachments = {4},
                //     .depth_attachment = {5},
                //     .execute = [&](CommandBuffer& commands) {}
                // }
            }
        }
    );

    // Bloom

    mip_chain_generator.fill_mip_chain(render_graph, lit_scene_handle, bloom_mip_chain);

    // Other postprocessing

    // TODO

    // UI

    const auto swapchain_index = backend.get_current_swapchain_index();
    const auto& swapchain_image = swapchain_images.at(swapchain_index);
    render_graph.add_render_pass(
        {
            .name = "UI",
            .textures = {
                {
                    lit_scene_handle, {
                        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    }
                },
                {
                    bloom_mip_chain, {
                        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    }
                }
            },
            .attachments = {swapchain_image},
            .subpasses = {
                {
                    .name = "UI",
                    .color_attachments = {0},
                    .execute = [&](CommandBuffer& commands) {
                        ui_phase.render(commands, player_view, bloom_mip_chain);
                    }
                }
            }
        }
    );

    mip_chain_generator.fill_mip_chain(render_graph, gbuffer_depth_handle, depth_buffer_mip_chain);
    mip_chain_generator.fill_mip_chain(render_graph, gbuffer_normals_handle, normal_target_mip_chain);

    render_graph.add_present_pass(
        {
            .swapchain_image = swapchain_image
        }
    );

    render_graph.finish();

    last_frame_depth_usage = render_graph.get_last_usage_token(depth_buffer_mip_chain);
    last_frame_normal_usage = render_graph.get_last_usage_token(normal_target_mip_chain);

    backend.execute_graph(std::move(render_graph));    
}

RenderBackend& SceneRenderer::get_backend() {
    return backend;
}

SceneTransform& SceneRenderer::get_local_player() {
    return player_view;
}

TextureLoader& SceneRenderer::get_texture_loader() {
    return texture_loader;
}

MaterialStorage& SceneRenderer::get_material_storage() {
    return material_storage;
}

void SceneRenderer::create_shadow_render_targets() {
    auto& allocator = backend.get_global_allocator();

    if (shadowmap_handle != TextureHandle::None) {
        allocator.destroy_texture(shadowmap_handle);
    }

    shadowmap_handle = allocator.create_texture(
        "Sun shadowmap", VK_FORMAT_D16_UNORM,
        glm::uvec2{
            cvar_shadow_cascade_resolution.Get(),
            cvar_shadow_cascade_resolution.Get()
        }, 1,
        TextureUsage::RenderTarget, cvar_num_shadow_cascades.Get()
    );

    lighting_pass.set_shadowmap(shadowmap_handle);
}

void SceneRenderer::create_scene_render_targets() {
    auto& allocator = backend.get_global_allocator();

    if (gbuffer_color_handle != TextureHandle::None) {
        allocator.destroy_texture(gbuffer_color_handle);
    }

    if (gbuffer_normals_handle != TextureHandle::None) {
        allocator.destroy_texture(gbuffer_normals_handle);
    }

    if (gbuffer_data_handle != TextureHandle::None) {
        allocator.destroy_texture(gbuffer_data_handle);
    }

    if (gbuffer_emission_handle != TextureHandle::None) {
        allocator.destroy_texture(gbuffer_emission_handle);
    }

    if (gbuffer_depth_handle != TextureHandle::None) {
        allocator.destroy_texture(gbuffer_depth_handle);
    }

    if (depth_buffer_mip_chain != TextureHandle::None) {
        allocator.destroy_texture(depth_buffer_mip_chain);
    }

    if (normal_target_mip_chain != TextureHandle::None) {
        allocator.destroy_texture(normal_target_mip_chain);
    }

    if (lit_scene_handle != TextureHandle::None) {
        allocator.destroy_texture(lit_scene_handle);
    }

    if (bloom_mip_chain != TextureHandle::None) {
        allocator.destroy_texture(bloom_mip_chain);
    }

    // gbuffer and lighting render targets
    gbuffer_color_handle = allocator.create_texture(
        "gbuffer_color", VK_FORMAT_R8G8B8A8_SRGB,
        scene_render_resolution,
        1, TextureUsage::RenderTarget
    );

    gbuffer_normals_handle = allocator.create_texture(
        "gbuffer_normals",
        VK_FORMAT_R16G16B16A16_SFLOAT,
        scene_render_resolution, 1,
        TextureUsage::RenderTarget
    );

    gbuffer_data_handle = allocator.create_texture(
        "gbuffer_data", VK_FORMAT_R8G8B8A8_UNORM,
        scene_render_resolution, 1,
        TextureUsage::RenderTarget
    );

    gbuffer_emission_handle = allocator.create_texture(
        "gbuffer_emission", VK_FORMAT_R8G8B8A8_SRGB,
        scene_render_resolution, 1,
        TextureUsage::RenderTarget
    );

    gbuffer_depth_handle = allocator.create_texture(
        "gbuffer_depth", VK_FORMAT_D32_SFLOAT,
        scene_render_resolution, 1,
        TextureUsage::RenderTarget
    );

    const auto mip_chain_resolution = scene_render_resolution / glm::uvec2{ 2 };
    const auto minor_dimension = glm::min(mip_chain_resolution.x, mip_chain_resolution.y);
    const auto num_mips = static_cast<uint32_t>(floor(log2(minor_dimension)));
    depth_buffer_mip_chain = allocator.create_texture(
        "Depth buffer mip chain", VK_FORMAT_R16_SFLOAT,
        mip_chain_resolution, num_mips,
        TextureUsage::StorageImage
    );

    normal_target_mip_chain = allocator.create_texture(
        "gbuffer_normals B", VK_FORMAT_R16G16B16A16_SFLOAT,
        mip_chain_resolution, num_mips,
        TextureUsage::StorageImage
    );

    lit_scene_handle = allocator.create_texture(
        "lit_scene", VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        scene_render_resolution, 1,
        TextureUsage::RenderTarget
    );

    bloom_mip_chain = allocator.create_texture(
        "Bloom mip chain", VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        mip_chain_resolution, num_mips,
        TextureUsage::StorageImage
    );

    auto& swapchain = backend.get_swapchain();
    const auto& images = swapchain.get_images();
    const auto& image_views = swapchain.get_image_views();
    for (auto swapchain_image_index = 0u; swapchain_image_index < swapchain.image_count; swapchain_image_index++) {
        const auto swapchain_image = allocator.emplace_texture(
            fmt::format("Swapchain image {}", swapchain_image_index), Texture{
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
            }
        );

        swapchain_images.push_back(swapchain_image);
    }

    ui_phase.set_resources(lit_scene_handle);
}

MeshStorage& SceneRenderer::get_mesh_storage() {
    return meshes;
}

tl::optional<VoxelCache&> SceneRenderer::get_voxel_cache() const {
    if (voxel_cache) {
        return *voxel_cache;
    } else {
        return tl::nullopt;
    }
}

void SceneRenderer::translate_player(const glm::vec3& movement) {
    player_view.translate(movement);
}
