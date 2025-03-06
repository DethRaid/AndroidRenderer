#include "fsr3.hpp"

#include "backend/render_backend.hpp"
#include "console/cvars.hpp"
#include "core/system_interface.hpp"

static auto cvar_fsr3_quality = AutoCVar_Enum{
    "r.FSR3.Quality", "FSR3 Quality", FFX_UPSCALE_QUALITY_MODE_QUALITY
};

static std::string to_string(FfxApiUpscaleQualityMode quality_mode);

static std::shared_ptr<spdlog::logger> logger;

FidelityFSSuperResolution3::FidelityFSSuperResolution3(RenderBackend& backend) {
    if(logger == nullptr) {
        logger = SystemInterface::get().get_logger("FidelityFSSuperResolution3");
    }

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

void FidelityFSSuperResolution3::initialize(const glm::uvec2& output_resolution) {
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

        if (has_context) {
            ffx::DestroyContext(upscaling_context);
            has_context = false;
        }
    }

    if (!has_context) {
        ffx::CreateContextDescUpscale create_upscaling;
        create_upscaling.flags = FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE |
            FFX_UPSCALE_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION |
            FFX_UPSCALE_ENABLE_AUTO_EXPOSURE | FFX_UPSCALE_ENABLE_DEBUG_CHECKING;
        create_upscaling.maxRenderSize = { .width = optimal_render_resolution.x, .height = optimal_render_resolution.y };
        create_upscaling.maxUpscaleSize = { .width = output_resolution.x, .height = output_resolution.y };

        result = ffx::CreateContext(upscaling_context, nullptr, create_upscaling, backend_desc);
        if (result != ffx::ReturnCode::Ok) {
            logger->error("Could not initialize FSR: {}");
        } else {
            has_context = true;
        }
    }
}

glm::uvec2 FidelityFSSuperResolution3::get_optimal_render_resolution() const {
    return optimal_render_resolution;
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
