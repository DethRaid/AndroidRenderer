#include "fsr3.hpp"

#if SAH_USE_FFX

#include <utf8.h>

#include "render/scene_view.hpp"
#include "render/backend/render_backend.hpp"
#include "console/cvars.hpp"
#include "core/system_interface.hpp"
#include "core/string_conversion.hpp"

static auto cvar_fsr3_quality = AutoCVar_Enum{
    "r.FSR3.Quality", "FSR3 Quality", FFX_UPSCALE_QUALITY_MODE_QUALITY
};

static std::string to_string(FfxApiUpscaleQualityMode quality_mode);

static std::shared_ptr<spdlog::logger> logger;

FidelityFSSuperResolution3::FidelityFSSuperResolution3() {
    if(logger == nullptr) {
        logger = SystemInterface::get().get_logger("FidelityFSSuperResolution3");
    }

    auto& backend = RenderBackend::get();

    backend_desc.vkDevice = backend.get_device();
    backend_desc.vkPhysicalDevice = backend.get_physical_device();
    backend_desc.vkDeviceProcAddr = vkGetDeviceProcAddr;
}

FidelityFSSuperResolution3::~FidelityFSSuperResolution3() {
    if(has_context) {
        ffx::DestroyContext(upscaling_context);
        has_context = false;
    }
}

void FidelityFSSuperResolution3::initialize(const glm::uvec2 output_resolution_in, const uint32_t frame_number) {
    output_resolution = output_resolution_in;
    glm::uvec2 new_render_resolution;
    ffx::QueryDescUpscaleGetRenderResolutionFromQualityMode query = {};
    query.displayWidth = output_resolution.x;
    query.displayHeight = output_resolution.y;
    query.qualityMode = static_cast<uint32_t>(cvar_fsr3_quality.Get());
    query.pOutRenderWidth = &new_render_resolution.x;
    query.pOutRenderHeight = &new_render_resolution.y;
    auto result = ffx::Query(query);

    if(new_render_resolution != optimal_render_resolution) {
        optimal_render_resolution = new_render_resolution;
        logger->info(
            "Rendering at {}x{} for output resolution {}x{} and quality mode {}",
            optimal_render_resolution.x,
            optimal_render_resolution.y,
            output_resolution.x,
            output_resolution.y,
            to_string(cvar_fsr3_quality.Get()));

        if(has_context) {
            ffx::DestroyContext(upscaling_context);
            has_context = false;
        }
    }

    if(!has_context) {
        ffx::CreateContextDescUpscale create_upscaling;
        create_upscaling.flags = FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE |
            FFX_UPSCALE_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION |
            FFX_UPSCALE_ENABLE_AUTO_EXPOSURE |
            FFX_UPSCALE_ENABLE_DEPTH_INFINITE |
            FFX_UPSCALE_ENABLE_DEBUG_CHECKING;
        create_upscaling.maxRenderSize = {.width = optimal_render_resolution.x, .height = optimal_render_resolution.y};
        create_upscaling.maxUpscaleSize = {.width = output_resolution.x, .height = output_resolution.y};
        create_upscaling.fpMessage = +[](const uint32_t type, const wchar_t* c_message) {
            const auto wmessage = std::u16string_view{reinterpret_cast<const char16_t*>(c_message)};
            const auto message = utf8::utf16tou8(wmessage);
            const auto skill_issue = std::string_view{reinterpret_cast<const char*>(message.c_str()), message.size()};
            if(type == FFX_API_MESSAGE_TYPE_WARNING) {
                logger->warn("{}", skill_issue);
            } else if(type == FFX_API_MESSAGE_TYPE_ERROR) {
                logger->error("{}", skill_issue);
            } else {
                logger->info("{}", skill_issue);
            }
        };

        result = ffx::CreateContext(upscaling_context, nullptr, create_upscaling, backend_desc);
        if(result != ffx::ReturnCode::Ok) {
            logger->error("Could not initialize FSR: {}");
        } else {
            has_context = true;
        }
    }

    auto jitter_phase_count = 0;
    auto jitter_phase_desc = ffx::QueryDescUpscaleGetJitterPhaseCount{};
    jitter_phase_desc.displayWidth = output_resolution.x;
    jitter_phase_desc.renderWidth = optimal_render_resolution.x;
    jitter_phase_desc.pOutPhaseCount = &jitter_phase_count;
    ffx::Query(upscaling_context, jitter_phase_desc);

    auto jitter_offset_desc = ffx::QueryDescUpscaleGetJitterOffset{};
    jitter_offset_desc.index = jitter_index;
    jitter_offset_desc.phaseCount = jitter_phase_count;
    jitter_offset_desc.pOutX = &jitter.x;
    jitter_offset_desc.pOutY = &jitter.y;
    ffx::Query(upscaling_context, jitter_offset_desc);

    jitter_index++;
    if(jitter_index >= jitter_phase_count) {
        jitter_index = 0;
    }
}

void FidelityFSSuperResolution3::set_constants(const SceneView& scene_transform, const glm::uvec2 render_resolution) {
    const auto jitter = scene_transform.get_jitter();
    dispatch_desc.jitterOffset = {-jitter.x, -jitter.y};
    dispatch_desc.motionVectorScale = {1.f, 1.f};
    dispatch_desc.renderSize = {render_resolution.x, render_resolution.y};
    dispatch_desc.upscaleSize = {output_resolution.x, output_resolution.y};
    dispatch_desc.frameTimeDelta = 7.5f; // Hardcoded, please fix
    dispatch_desc.preExposure = 1.f;
    dispatch_desc.cameraNear = scene_transform.get_near();
    dispatch_desc.cameraFar = FLT_MAX;
    dispatch_desc.cameraFovAngleVertical = glm::radians(scene_transform.get_fov());
    dispatch_desc.viewSpaceToMetersFactor = 1.f;
}

void FidelityFSSuperResolution3::evaluate(
    RenderGraph& graph, const TextureHandle color_in, const TextureHandle color_out,
    const TextureHandle depth_in, const TextureHandle motion_vectors_in
) {
    graph.add_pass(
        {
            .name = "fsr3",
            .textures = {
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
                    .texture = depth_in,
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
            },
            .execute = [&](CommandBuffer& commands) {
                const auto color_in_res = FfxApiResource{
                    .resource = color_in->image,
                    .description = ffxApiGetImageResourceDescriptionVK(color_in->image, color_in->create_info, 0),
                    .state = FFX_API_RESOURCE_STATE_COMPUTE_READ
                };
                const auto color_out_res = FfxApiResource{
                    .resource = color_out->image,
                    .description = ffxApiGetImageResourceDescriptionVK(color_out->image, color_out->create_info, 0),
                    .state = FFX_API_RESOURCE_STATE_UNORDERED_ACCESS
                };
                const auto depth_in_res = FfxApiResource{
                    .resource = depth_in->image,
                    .description = ffxApiGetImageResourceDescriptionVK(depth_in->image, depth_in->create_info, 0),
                    .state = FFX_API_RESOURCE_STATE_COMPUTE_READ
                };
                const auto motion_vectors_in_res = FfxApiResource{
                    .resource = motion_vectors_in->image,
                    .description = ffxApiGetImageResourceDescriptionVK(
                        motion_vectors_in->image,
                        motion_vectors_in->create_info,
                        0),
                    .state = FFX_API_RESOURCE_STATE_COMPUTE_READ
                };

                auto local_dispatch_desc = dispatch_desc;
                local_dispatch_desc.commandList = commands.get_vk_commands();
                local_dispatch_desc.color = color_in_res;
                local_dispatch_desc.depth = depth_in_res;
                local_dispatch_desc.motionVectors = motion_vectors_in_res;
                local_dispatch_desc.output = color_out_res;
                // local_dispatch_desc.flags = FFX_UPSCALE_FLAG_DRAW_DEBUG_VIEW;

                ffx::Dispatch(upscaling_context, local_dispatch_desc);
            }
        });
}

glm::uvec2 FidelityFSSuperResolution3::get_optimal_render_resolution() const {
    return optimal_render_resolution;
}

glm::vec2 FidelityFSSuperResolution3::get_jitter() {
    return jitter;
}

std::string to_string(const FfxApiUpscaleQualityMode quality_mode) {
    switch(quality_mode) {
    case FFX_UPSCALE_QUALITY_MODE_NATIVEAA:
        return "Native AA";
    case FFX_UPSCALE_QUALITY_MODE_QUALITY:
        return "Quality";
    case FFX_UPSCALE_QUALITY_MODE_BALANCED:
        return "Balanced";
    case FFX_UPSCALE_QUALITY_MODE_PERFORMANCE:
        return "Performance";
    case FFX_UPSCALE_QUALITY_MODE_ULTRA_PERFORMANCE:
        return "Ultra Performance";
    }

    return "Unknown";
}

#endif
