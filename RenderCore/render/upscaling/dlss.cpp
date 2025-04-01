#include "dlss.hpp"

#include "render/gbuffer.hpp"

#if SAH_USE_STREAMLINE

#include <stdexcept>

#include <sl_helpers.h>
#include <sl_dlss_d.h>

#include "core/system_interface.hpp"
#include "console/cvars.hpp"
#include "render/scene_view.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/render_graph.hpp"

static auto cvar_dlss_quality = AutoCVar_Enum{
    "r.DLSS.Quality", "DLSS Quality", sl::DLSSMode::eMaxQuality
};

static auto cvar_ray_reconstruction = AutoCVar_Int{
    "r.DLSS-RR.Enabled", "Whether to enable DLSS Ray Reconstruction", true
};

static std::shared_ptr<spdlog::logger> logger;

static sl::Resource wrap_resource(TextureHandle texture, VkImageLayout layout);

DLSSAdapter::DLSSAdapter() {
    if(logger == nullptr) {
        logger = SystemInterface::get().get_logger("DLSS");
    }

    bool dlss_loaded = false;
    slIsFeatureLoaded(sl::kFeatureDLSS, dlss_loaded);
    if(!dlss_loaded) {
        auto result = slSetFeatureLoaded(sl::kFeatureDLSS, true);
        if(result != sl::Result::eOk) {
            logger->error("Error loading DLSS: {}", sl::getResultAsStr(result));
            throw std::runtime_error{"Could not load DLSS!"};
        }
    }

    bool dlss_rr_loaded = false;
    slIsFeatureLoaded(sl::kFeatureDLSS_RR, dlss_rr_loaded);
    if(!dlss_rr_loaded) {
        auto result = slSetFeatureLoaded(sl::kFeatureDLSS_RR, true);
        if(result != sl::Result::eOk) {
            logger->warn("Error loading DLSS-RR: {}", sl::getResultAsStr(result));
        }
    }
}

DLSSAdapter::~DLSSAdapter() {
    RenderBackend::get().wait_for_idle();

    slSetFeatureLoaded(sl::kFeatureDLSS, false);
}

void DLSSAdapter::initialize(const glm::uvec2 output_resolution, const uint32_t frame_index) {
    slGetNewFrameToken(frame_token, &frame_index);

    dlss_mode = cvar_dlss_quality.Get();

    sl::DLSSOptions dlss_options;
    dlss_options.mode = dlss_mode;
    dlss_options.outputWidth = output_resolution.x;
    dlss_options.outputHeight = output_resolution.y;

    auto result = slDLSSGetOptimalSettings(dlss_options, dlss_settings);
    if(result != sl::Result::eOk) {
        logger->error("Error getting DLSS settings: {}", sl::getResultAsStr(result));
    }
}

glm::uvec2 DLSSAdapter::get_optimal_render_resolution() const {
    return glm::uvec2{dlss_settings.optimalRenderWidth, dlss_settings.optimalRenderHeight};
}

void DLSSAdapter::set_constants(const SceneView& scene_transform, const glm::uvec2 render_resolution) {
    const auto& view_data = scene_transform.get_gpu_data();

    auto constants = sl::Constants{};

    auto projection = scene_transform.get_projection();

    const auto inverse_projection = glm::inverse(projection);

    std::memcpy(&constants.cameraViewToClip, &projection, sizeof(glm::mat4));
    std::memcpy(&constants.clipToCameraView, &inverse_projection, sizeof(glm::mat4));

    const auto clip_to_prev_clip = inverse_projection * view_data.inverse_view * view_data.last_frame_view *
        scene_transform.get_last_frame_projection();
    std::memcpy(&constants.clipToPrevClip, &clip_to_prev_clip, sizeof(glm::mat4));

    const auto prev_clip_to_clip = glm::inverse(clip_to_prev_clip);
    std::memcpy(&constants.prevClipToClip, &prev_clip_to_clip, sizeof(glm::mat4));

    const auto jitter = scene_transform.get_jitter();
    constants.jitterOffset = {-jitter.x, -jitter.y};

    constants.mvecScale = {
        1.f / static_cast<float>(render_resolution.x), 1.f / static_cast<float>(render_resolution.y)
    };

    constants.cameraPinholeOffset = {0, 0};

    const auto camera_pos = scene_transform.get_position();
    std::memcpy(&constants.cameraPos, &camera_pos, sizeof(glm::vec3));

    const auto camera_up = glm::vec3{0, 1, 0} * glm::mat3{view_data.inverse_view};
    constants.cameraUp = {camera_up.x, camera_up.y, camera_up.z};

    const auto camera_right = glm::vec3{1, 0, 0} * glm::mat3{view_data.inverse_view};
    constants.cameraRight = {camera_right.x, camera_up.y, camera_up.z};

    const auto camera_forward = scene_transform.get_forward();
    constants.cameraFwd = {camera_forward.x, camera_forward.y, camera_forward.z};

    constants.cameraNear = scene_transform.get_near();
    constants.cameraFar = 65536.f;

    constants.cameraFOV = scene_transform.get_fov();

    constants.cameraAspectRatio = scene_transform.get_aspect_ratio();

    constants.depthInverted = sl::Boolean::eTrue;
    constants.cameraMotionIncluded = sl::Boolean::eTrue;
    constants.motionVectors3D = sl::Boolean::eFalse;
    constants.reset = sl::Boolean::eFalse;
    constants.orthographicProjection = sl::Boolean::eFalse;
    constants.motionVectorsJittered = sl::Boolean::eTrue;

    slSetConstants(constants, *frame_token, viewport);
}

void DLSSAdapter::evaluate(
    RenderGraph& graph, const SceneView& view, const GBuffer& gbuffer,
    const TextureHandle color_in, const TextureHandle color_out,
    const TextureHandle motion_vectors_in
) {
    auto textures = TextureUsageList{
        {
            .texture = color_in,
            .stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .access = VK_ACCESS_2_SHADER_READ_BIT,
            .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        },
        {
            .texture = color_out,
            .stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .access = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
            .layout = VK_IMAGE_LAYOUT_GENERAL
        },
        {
            .texture = gbuffer.depth,
            .stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .access = VK_ACCESS_2_SHADER_READ_BIT,
            .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        },
        {
            .texture = motion_vectors_in,
            .stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .access = VK_ACCESS_2_SHADER_READ_BIT,
            .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        },
    };

    if(cvar_ray_reconstruction.Get() != 0) {
        pack_dlss_rr_inputs(graph, gbuffer);

        textures.emplace_back(
            diffuse_albedo,
            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        textures.emplace_back(
            specular_albedo,
            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        textures.emplace_back(
            packed_normals_roughness,
            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    graph.add_pass(
        {
            .name = "dlss",
            .textures = textures,
            .execute = [&](CommandBuffer& commands) {
                auto color_in_res = wrap_resource(color_in, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                auto color_out_res = wrap_resource(color_out, VK_IMAGE_LAYOUT_GENERAL);
                auto depth_in_res = wrap_resource(gbuffer.depth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                auto motion_vectors_in_res = wrap_resource(
                    motion_vectors_in,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                auto tags = eastl::vector<sl::ResourceTag>{};
                tags.reserve(8);

                tags.emplace_back(
                    &color_in_res,
                    sl::kBufferTypeScalingInputColor,
                    sl::ResourceLifecycle::eValidUntilPresent);
                tags.emplace_back(
                    &color_out_res,
                    sl::kBufferTypeScalingOutputColor,
                    sl::ResourceLifecycle::eValidUntilPresent);
                tags.emplace_back(&depth_in_res, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent);
                tags.emplace_back(
                    &motion_vectors_in_res,
                    sl::kBufferTypeMotionVectors,
                    sl::ResourceLifecycle::eValidUntilPresent);

                if(diffuse_albedo != nullptr) {
                    tags.emplace_back(
                        &sl_diffuse_albedo,
                        sl::kBufferTypeAlbedo,
                        sl::ResourceLifecycle::eValidUntilPresent);
                }
                if(specular_albedo != nullptr) {
                    tags.emplace_back(
                        &sl_specular_albedo,
                        sl::kBufferTypeSpecularAlbedo,
                        sl::ResourceLifecycle::eValidUntilPresent);
                }
                if(packed_normals_roughness != nullptr) {
                    tags.emplace_back(
                        &sl_normals_roughness,
                        sl::kBufferTypeNormalRoughness,
                        sl::ResourceLifecycle::eValidUntilPresent);
                }

                slSetTag(viewport, tags.data(), static_cast<uint32_t>(tags.size()), commands.get_vk_commands());

                const auto& output_resolution = color_out->get_resolution();

                auto feature = sl::kFeatureDLSS;
                if(cvar_ray_reconstruction.Get() != 0) {
                    auto dlssd_options = sl::DLSSDOptions{};
                    dlssd_options.mode = dlss_mode;
                    dlssd_options.outputWidth = output_resolution.x;
                    dlssd_options.outputHeight = output_resolution.y;
                    dlssd_options.sharpness = dlss_settings.optimalSharpness;
                    dlssd_options.normalRoughnessMode = sl::DLSSDNormalRoughnessMode::ePacked;

                    const auto world_to_view = view.get_view();
                    std::memcpy(&dlssd_options.worldToCameraView, &world_to_view, sizeof(float4x4));

                    const auto view_to_world = inverse(world_to_view);
                    std::memcpy(&dlssd_options.cameraViewToWorld, &view_to_world, sizeof(float4x4));

                    slDLSSDSetOptions(viewport, dlssd_options);

                    feature = sl::kFeatureDLSS_RR;

                } else {
                    auto options = sl::DLSSOptions{};
                    options.mode = dlss_mode;
                    options.outputWidth = output_resolution.x;
                    options.outputHeight = output_resolution.y;
                    options.sharpness = dlss_settings.optimalSharpness;
                    options.useAutoExposure = sl::Boolean::eFalse;
                    slDLSSSetOptions(viewport, options);
                }

                auto options_arr = eastl::array<const sl::BaseStructure*, 1>{&viewport};
                const auto result = slEvaluateFeature(
                    feature,
                    *frame_token,
                    options_arr.data(),
                    options_arr.size(),
                    commands.get_vk_commands());
                if(result != sl::Result::eOk) {
                    logger->error("Error evaluating DLSS: {}", sl::getResultAsStr(result));
                }
            }
        });
}

void DLSSAdapter::pack_dlss_rr_inputs(RenderGraph& graph, const GBuffer& gbuffer) {
    auto& backend = RenderBackend::get();

    if(dlss_rr_packing_pipeline == nullptr) {
        dlss_rr_packing_pipeline = backend.begin_building_pipeline("dlss_rr_input_packing")
            .set_vertex_shader("shaders/common/fullscreen.vert.spv")
            .set_fragment_shader("shaders/dlss/dlss_rr_packing.frag.spv")
            .set_depth_state({ .enable_depth_test = false, .enable_depth_write = false })
            .set_num_attachments(3)
                                          .build();
    }

    auto& allocator = backend.get_global_allocator();
    if(diffuse_albedo == nullptr) {
        diffuse_albedo = allocator.create_texture(
            "dlssrr_diffuse_albedo",
            {
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .resolution = gbuffer.color->get_resolution(),
                .usage = TextureUsage::RenderTarget,
            });
        sl_diffuse_albedo = wrap_resource(diffuse_albedo, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    if(specular_albedo == nullptr) {
        specular_albedo = allocator.create_texture(
            "dlssrr_specular_albedo",
            {
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .resolution = gbuffer.color->get_resolution(),
                .usage = TextureUsage::RenderTarget,
            });
        sl_specular_albedo = wrap_resource(specular_albedo, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    if(packed_normals_roughness == nullptr) {
        packed_normals_roughness = allocator.create_texture(
            "dlssrr_normals_roughness",
            {
                .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                .resolution = gbuffer.color->get_resolution(),
                .usage = TextureUsage::RenderTarget,
            });
        sl_normals_roughness = wrap_resource(packed_normals_roughness, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    const auto set = backend.get_transient_descriptor_allocator().build_set(dlss_rr_packing_pipeline, 0)
                            .bind(gbuffer.color)
                            .bind(gbuffer.normals)
                            .bind(gbuffer.data)
                            .build();

    graph.add_render_pass(
        {
            .name = "pack_dlss_rr_inputs",
            .descriptor_sets = {set},
            .color_attachments = {
                {
                    .image = diffuse_albedo,
                    .load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE
                },
                {
                    .image = specular_albedo,
                    .load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE
                },
                {
                    .image = packed_normals_roughness,
                    .load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE
                },
            },
            .execute = [&](CommandBuffer& commands) {
                commands.bind_descriptor_set(0, set);
                commands.bind_pipeline(dlss_rr_packing_pipeline);
                commands.draw_triangle();
            }
        });
}

sl::Resource wrap_resource(const TextureHandle texture, const VkImageLayout layout) {
    auto sl_resource = sl::Resource{
        sl::ResourceType::eTex2d,
        texture->image,
        texture->vma.allocation_info.deviceMemory,
        texture->image_view,
        static_cast<uint32_t>(layout)
    };
    const auto extent = texture->create_info.extent;
    sl_resource.width = extent.width;
    sl_resource.height = extent.height;
    sl_resource.nativeFormat = texture->create_info.format;
    // sl_resource.mipLevels = texture->create_info.mipLevels;
    // sl_resource.arrayLayers = texture->create_info.arrayLayers;
    // sl_resource.flags = texture->create_info.flags;
    // sl_resource.usage = texture->create_info.usage;

    return sl_resource;
}
#endif
