#include <spdlog/logger.h>
#include <spdlog/sinks/android_sink.h>

#include "gltf/gltf_model.hpp"
#include "scene_renderer.hpp"

#include "backend/render_graph.hpp"
#include "core/system_interface.hpp"
#include "render/backend/framebuffer.hpp"
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
    backend{}, player_view{backend}, texture_loader{backend}, materials{backend},
    meshes{backend.get_global_allocator(), backend.get_upload_queue()}, lpv{backend},
    voxelizer{backend, glm::uvec3{32, 32, 32}}, lighting_pass{backend}, ui_phase{*this} {
    logger = SystemInterface::get().get_logger("SceneRenderer");

    player_view.set_position_and_direction(glm::vec3{7.f, 1.f, -0.25f}, glm::vec3{-1.f, 0.0f, 0.f});

    const auto render_resolution = SystemInterface::get().get_resolution();

    player_view.set_perspective_projection(
        75.f, static_cast<float>(render_resolution.y) /
        static_cast<float>(render_resolution.x), 0.05f
    );

    create_render_passes();

    create_shadow_render_targets();

    set_render_resolution(render_resolution);

    lpv.init_resources(backend.get_global_allocator());

    logger->info("Initialized SceneRenderer");
}

void SceneRenderer::set_render_resolution(const glm::uvec2& resolution) {
    if (resolution == scene_render_resolution) {
        return;
    }

    logger->info("Setting resolution to {} by {}", resolution.x, resolution.y);

    scene_render_resolution = resolution;

    player_view.set_render_resolution(scene_render_resolution);

    player_view.set_aspect_ratio(
        static_cast<float>(scene_render_resolution.x) / static_cast<float>(scene_render_resolution.y)
    );

    create_scene_render_targets_and_framebuffers();
}

void SceneRenderer::set_scene(RenderScene& scene_in) {
    scene = &scene_in;
    lighting_pass.set_scene(scene_in);

    sun_shadow_drawer = scene->create_view(ScenePassType::Shadow, meshes);
    gbuffer_drawer = scene->create_view(ScenePassType::Gbuffer, meshes);

    lpv.set_rsm_view(scene->create_view(ScenePassType::RSM, meshes));
}

void SceneRenderer::render() {
    ZoneScoped;

    backend.begin_frame();

    auto render_graph = RenderGraph{backend};

    render_graph.add_compute_pass(
        ComputePass{
            .name = "Begin Frame",
            .execute = [&](CommandBuffer& commands) {
                ZoneScopedN("Begin Frame");
                GpuZoneScopedN(commands, "Begin Frame");

                auto& sun = scene->get_sun_light();
                sun.update_shadow_cascades(player_view);
                sun.update_buffer(commands);

                player_view.update_transforms(commands);

                lpv.update_cascade_transforms(player_view, scene->get_sun_light());
                lpv.update_buffers(commands);

                scene->flush_primitive_upload(commands);

                materials.flush_material_buffer(commands);
            }
        }
    );

    lpv.add_clear_volume_pass(render_graph);

    render_graph.add_transition_pass(
        {
            .buffers = {
                {
                    scene->get_primitive_buffer(),
                    {.stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, .access = VK_ACCESS_SHADER_READ_BIT}
                }
            }
        }
    );

    // VPL cloud generation

    lpv.inject_indirect_sun_light(render_graph, *scene, meshes);

    // Shadows
    // Render shadow pass after RSM so the shadow VS can overlap with the VPL FS
    // TODO: Multiview
    render_graph.add_render_pass(
        RenderPass{
            .name = "CSM sun shadow",
            .render_targets = {shadowmap_handle},
            .clear_values = std::vector{
                VkClearValue{.depthStencil = {.depth = 1.f}}
            },
            .subpasses = {
                Subpass{
                    .name = "Sun shadow",
                    .depth_attachment = {0},
                    .execute = [&](CommandBuffer& commands) {
                        ZoneScopedN("Sun Shadow");
                        GpuZoneScopedN(commands, "Sun Shadow");

                        auto& sun = scene->get_sun_light();

                        auto global_set = *backend.create_frame_descriptor_builder()
                                                  .bind_buffer(
                                                      0, {
                                                          .buffer = sun.get_constant_buffer()
                                                      }, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT
                                                  )
                                                  .bind_buffer(
                                                      1, {
                                                          .buffer = scene->get_primitive_buffer(),
                                                      }, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT
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
            .render_targets = std::vector{
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
                        ZoneScopedN("GBuffer");
                        GpuZoneScopedN(commands, "GBuffer");

                        auto global_set = *backend.create_frame_descriptor_builder()
                                                  .bind_buffer(
                                                      0, {
                                                          .buffer = player_view.get_buffer()
                                                      }, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT
                                                  )
                                                  .bind_buffer(
                                                      1, {
                                                          .buffer = scene->get_primitive_buffer(),
                                                      }, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT
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

    // Other postprocessing

    const auto swapchain_index = backend.get_current_swapchain_index();
    const auto& swapchain_image = swapchain_images.at(swapchain_index);
    render_graph.add_render_pass(
        {
            .name = "UI",
            .textures = {
                {
                    lit_scene_handle, {
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    }
                }
            },
            .render_targets = {swapchain_image},
            .subpasses = {
                {
                    .name = "UI",
                    .color_attachments = {0},
                    .execute = [&](CommandBuffer& commands) {
                        ui_phase.render(commands, player_view);
                    }
                }
            }
        }
    );

    render_graph.add_compute_pass(
        {
            .name = "Tracy Collect",
            .execute = [&](CommandBuffer& commands) { backend.collect_tracy_data(commands); }
        }
    );

    render_graph.add_transition_pass(
        {
            .textures = {
                {
                    swapchain_image,
                    {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR}
                }
            }
        }
    );

    render_graph.finish();

    backend.end_frame();
}

TracyVkCtx SceneRenderer::get_tracy_context() {
    return backend.get_tracy_context();
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

void SceneRenderer::create_render_passes() {
    /*
     * Creates the renderpasses we'll need
     *
     * # Sun shadow
     *
     * The sun shadow renderpass only two subpasses
     * - Shadowmap + RSM
     * - Build VPL lists
     *
     * The shadowmap + RSM pass is straightforward. We render the shadowmap using normal CSM techniques. We also
     * render the RSM flux and normal targets at the same time. This will hopefully save on something
     *
     * Building the VPL lists happens in fragment shaders, so we can keep the RSM targets in tile memory
     *
     * We inject the VPLs into the LPVs in compute shaders
     *
     * # Scene Render
     *
     * The scene render renderpass has a few subpasses:
     * - Opaque gbuffer
     * - Opaque lighting
     * - Translucency
     *
     * Opaque gbuffer is rendered with depth testing, depth writing, and no blending
     *
     * Opaque lighting renders each light with additive blending
     *
     * Translucency is rendered onto the lighting buffer using simple blending
     *
     * # Bloom
     * We don't actually have a renderpass for bloom, it's all done in compute
     *
     * # Postprocessing
     * One pass for tonemapping and color grading. Uses fragment shaders so it can overlap with next frame's shadow pass
     */

    // CSM sun shadow
    {
        const auto attachments = std::array{
            // Shadowmap
            VkAttachmentDescription{
                .format = VK_FORMAT_D16_UNORM,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            }
        };

        const auto depth_attachment = VkAttachmentReference{
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };

        const auto subpasses = std::array{
            // Shadow
            VkSubpassDescription{
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .pDepthStencilAttachment = &depth_attachment,
            },
        };

        // TODO: Create this dynamically based on the number of sun shadow cascades?
        const auto view_mask = 1u | 2u | 4u | 8u;

        auto view_masks = std::vector<uint32_t>{};
        view_masks.reserve(subpasses.size());

        for (uint32_t i = 0; i < subpasses.size(); i++) {
            view_masks.push_back(view_mask);
        }

        const auto multiview_info = VkRenderPassMultiviewCreateInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
            .subpassCount = static_cast<uint32_t>(view_masks.size()),
            .pViewMasks = view_masks.data()
        };

        const auto create_info = VkRenderPassCreateInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext = &multiview_info,
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),
            .subpassCount = static_cast<uint32_t>(subpasses.size()),
            .pSubpasses = subpasses.data(),
        };

        const auto result = vkCreateRenderPass(backend.get_device().device, &create_info, nullptr, &shadow_render_pass);
        if (result != VK_SUCCESS) {
            logger->error("Could not create shadow renderpass. Vulkan error {}", result);
        } else {
            logger->info("Shadow renderpass created!");
        }
    }

    // Gbuffer and lighting pass
    {
        const auto attachments = std::array{
            // gbuffer color
            VkAttachmentDescription{
                .format = VK_FORMAT_R8G8B8A8_SRGB,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },

            // gbuffer normals
            VkAttachmentDescription{
                .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },

            // gbuffer data
            VkAttachmentDescription{
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },

            // gbuffer emission
            VkAttachmentDescription{
                .format = VK_FORMAT_R8G8B8A8_SRGB,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },

            // Lit scene
            VkAttachmentDescription{
                .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },

            // Depth target
            VkAttachmentDescription{
                .format = VK_FORMAT_D32_SFLOAT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            },
        };

        const auto gbuffer_attachments = std::array{
            // Color
            VkAttachmentReference{
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            // Normals
            VkAttachmentReference{
                .attachment = 1,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            // Data
            VkAttachmentReference{
                .attachment = 2,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            // Emission
            VkAttachmentReference{
                .attachment = 3,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
        };
        const auto gbuffer_depth_attachment = VkAttachmentReference{
            .attachment = 5,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };

        const auto lighting_input_attachments = std::array{
            // Color
            VkAttachmentReference{
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            // Normals
            VkAttachmentReference{
                .attachment = 1,
                .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            // Data
            VkAttachmentReference{
                .attachment = 2,
                .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            // Emission
            VkAttachmentReference{
                .attachment = 3,
                .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            // Depth
            VkAttachmentReference{
                .attachment = 5,
                .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            },
        };

        const auto lighting_attachment = VkAttachmentReference{
            .attachment = 4,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };

        const auto translucency_depth_attachment = VkAttachmentReference{
            .attachment = 5,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        };

        const auto subpasses = std::array{
            // Gbuffer
            VkSubpassDescription{
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = static_cast<uint32_t>(gbuffer_attachments.size()),
                .pColorAttachments = gbuffer_attachments.data(),
                .pDepthStencilAttachment = &gbuffer_depth_attachment,
            },

            // Lighting
            VkSubpassDescription{
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .inputAttachmentCount = static_cast<uint32_t>(lighting_input_attachments.size()),
                .pInputAttachments = lighting_input_attachments.data(),
                .colorAttachmentCount = 1,
                .pColorAttachments = &lighting_attachment,
            },

            // Translucency
            VkSubpassDescription{
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = 1,
                .pColorAttachments = &lighting_attachment,
                .pDepthStencilAttachment = &translucency_depth_attachment,
            }
        };

        const auto dependencies = std::array{
            // Lighting pass depends on gbuffer
            VkSubpassDependency{
                .srcSubpass = 0,
                .dstSubpass = 1,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            },

            // Translucent pass depends on lighting
            VkSubpassDependency{
                .srcSubpass = 1,
                .dstSubpass = 2,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            },
        };

        auto create_info = VkRenderPassCreateInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),
            .subpassCount = static_cast<uint32_t>(subpasses.size()),
            .pSubpasses = subpasses.data(),
            .dependencyCount = static_cast<uint32_t>(dependencies.size()),
            .pDependencies = dependencies.data(),
        };

        const auto result = vkCreateRenderPass(
            backend.get_device().device, &create_info, nullptr,
            &scene_render_pass
        );
        if (result != VK_SUCCESS) {
            logger->error("Could not create scene renderpass. Vulkan error {}", result);
        } else {
            logger->info("Scene renderpass created!");
        }
    }

    // Postprocessing

    // Upscale and UI
    {
        const auto attachments = std::array{
            // Swapchain image
            VkAttachmentDescription{
                .format = backend.get_swapchain().image_format,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
        };

        const auto swapchain_attachments = std::array{
            // Swapchain
            VkAttachmentReference{
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
        };

        const auto subpasses = std::array{
            // Upscale/UI
            VkSubpassDescription{
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = static_cast<uint32_t>(swapchain_attachments.size()),
                .pColorAttachments = swapchain_attachments.data(),
            },
        };

        const auto dependencies = std::array{
            // External pass depends on UI pass
            VkSubpassDependency{
                .srcSubpass = 0,
                .dstSubpass = VK_SUBPASS_EXTERNAL,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            },
        };

        auto create_info = VkRenderPassCreateInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),
            .subpassCount = static_cast<uint32_t>(subpasses.size()),
            .pSubpasses = subpasses.data(),
            .dependencyCount = static_cast<uint32_t>(dependencies.size()),
            .pDependencies = dependencies.data(),
        };

        const auto result = vkCreateRenderPass(
            backend.get_device().device, &create_info, nullptr,
            &ui_render_pass
        );
        if (result != VK_SUCCESS) {
            logger->error("Could not create UI renderpass. Vulkan error {}", result);
        } else {
            logger->info("UI renderpass created!");
        }
    }
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

void SceneRenderer::create_scene_render_targets_and_framebuffers() {
    auto device = backend.get_device().device;
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

    if (lit_scene_handle != TextureHandle::None) {
        allocator.destroy_texture(lit_scene_handle);
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

    lit_scene_handle = allocator.create_texture(
        "lit_scene", VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        scene_render_resolution, 1,
        TextureUsage::RenderTarget
    );

    auto& swapchain = backend.get_swapchain();
    const auto& images = swapchain.get_images();
    const auto& image_views = swapchain.get_image_views();
    for (auto swapchain_image_index = 0; swapchain_image_index < swapchain.image_count; swapchain_image_index++) {
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

    lighting_pass.set_gbuffer(
        GBuffer{
            .color = gbuffer_color_handle,
            .normal = gbuffer_normals_handle,
            .data = gbuffer_data_handle,
            .emission = gbuffer_emission_handle,
            .depth = gbuffer_depth_handle,
        }
    );

    ui_phase.set_resources(lit_scene_handle);
}

MaterialStorage& SceneRenderer::get_material_storage() {
    return materials;
}

MeshStorage& SceneRenderer::get_mesh_storage() {
    return meshes;
}
