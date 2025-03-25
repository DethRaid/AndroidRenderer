#include "xess.hpp"

#include "core/system_interface.hpp"
#include "render/gbuffer.hpp"
#include "render/scene_view.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/utils.hpp"
#include "render/phase/motion_vectors_phase.hpp"

#if SAH_USE_XESS
#include <xess/xess_vk.h>
#include <xess/xess_vk_debug.h>

#include "console/cvars.hpp"

static bool xess_supported = true;

static std::shared_ptr<spdlog::logger> logger;

static AutoCVar_Enum cvar_xess_mode{
    "r.XeSS.Mode",
    "XeSS quality mode\n\t100 = Ultra Performance (33% render scale)\n\t101 = Performance (43% render scale)\n\t102 = Balanced (50% render scale)\n\t103 = Quality (59% render scale)\n\t104 = Ultra Quality (66% render scale)\n\t105 = Ultra Quality Plus (77% render scale)\n\t106 = Native-res Anti-Aliasing",
    XESS_QUALITY_SETTING_AA
};

eastl::vector<std::string> XeSSAdapter::get_instance_extensions() {
    uint32_t instance_extension_count;
    const char* const* instance_extensions;
    uint32_t api_version;

    const auto result = xessVKGetRequiredInstanceExtensions(
        &instance_extension_count,
        &instance_extensions,
        &api_version);
    if(result == XESS_RESULT_ERROR_UNSUPPORTED_DRIVER) {
        xess_supported = false;
        return {};
    }

    auto extensions = eastl::vector<std::string>{};
    extensions.reserve(instance_extension_count);

    for(auto i = 0u; i < instance_extension_count; i++) {
        extensions.emplace_back(instance_extensions[i]);
    }

    return extensions;
}

eastl::vector<std::string> XeSSAdapter::get_device_extensions(
    const VkInstance instance, const VkPhysicalDevice physical_device
) {
    uint32_t device_extension_count;
    const char* const* device_extensions;

    const auto result = xessVKGetRequiredDeviceExtensions(
        instance,
        physical_device,
        &device_extension_count,
        &device_extensions);
    if(result == XESS_RESULT_ERROR_UNSUPPORTED_DRIVER) {
        xess_supported = false;
        return {};
    }

    auto extensions = eastl::vector<std::string>{};
    extensions.reserve(device_extension_count);

    for(auto i = 0u; i < device_extension_count; i++) {
        extensions.emplace_back(device_extensions[i]);
    }

    return extensions;
}

void XeSSAdapter::add_required_features(
    const VkInstance instance, const VkPhysicalDevice physical_device, VkPhysicalDeviceFeatures2& features
) {
    void* p_features = &features;
    xessVKGetRequiredDeviceFeatures(instance, physical_device, &p_features);
}

static void xess_log(const char* message, xess_logging_level_t loggingLevel) {
    switch(loggingLevel) {
    case XESS_LOGGING_LEVEL_DEBUG:
        logger->debug(message);
        break;
    case XESS_LOGGING_LEVEL_INFO:
        logger->info(message);
        break;
    case XESS_LOGGING_LEVEL_WARNING:
        logger->warn(message);
        break;
    case XESS_LOGGING_LEVEL_ERROR:
        logger->error(message);
        break;
    }
}

XeSSAdapter::XeSSAdapter() {
    ZoneScoped;

    if(logger == nullptr) {
        logger = SystemInterface::get().get_logger("XeSS");
    }

    auto& backend = RenderBackend::get();
    auto result = xessVKCreateContext(
        backend.get_instance(),
        backend.get_physical_device(),
        backend.get_device(),
        &context);
    if(result != XESS_RESULT_SUCCESS) {
        logger->error("Could not create XeSS context: {}", result);
    }

    xessSetLoggingCallback(context, XESS_LOGGING_LEVEL_DEBUG, xess_log);

    //xessVKBuildPipelines(context, VK_NULL_HANDLE, true, XESS_INIT_FLAG_JITTERED_MV);
}

XeSSAdapter::~XeSSAdapter() {
    RenderBackend::get().wait_for_idle();

    xessDestroyContext(context);
}

void XeSSAdapter::initialize(const glm::uvec2 output_resolution, const uint32_t frame_index) {
    if(output_resolution != cached_output_resolution || cvar_xess_mode.Get() != cached_quality_mode) {
        RenderBackend::get().wait_for_idle();

        cached_output_resolution = output_resolution;
        cached_quality_mode = cvar_xess_mode.Get();

        uint32_t flags = XESS_INIT_FLAG_JITTERED_MV | XESS_INIT_FLAG_INVERTED_DEPTH;
        if(MotionVectorsPhase::render_full_res()) {
            flags |= XESS_INIT_FLAG_HIGH_RES_MV;
        }

        auto init_params = xess_vk_init_params_t{
            .outputResolution = {cached_output_resolution.x, cached_output_resolution.y},
            .qualitySetting = cvar_xess_mode.Get(),
            .initFlags = flags
        };

        auto init_result = xessVKInit(context, &init_params);
        if(init_result != XESS_RESULT_SUCCESS) {
            logger->error("Could not initialize XeSS: {}", init_result);
        }
    }

    const auto x_output_resolution = xess_2d_t{output_resolution.x, output_resolution.y};
    xessGetOptimalInputResolution(
        context,
        &x_output_resolution,
        cached_quality_mode,
        &optimal_input_resolution,
        &min_input_resolution,
        &max_input_resolution);
}

glm::uvec2 XeSSAdapter::get_optimal_render_resolution() const {
    return {optimal_input_resolution.x, optimal_input_resolution.y};
}

void XeSSAdapter::set_constants(const SceneView& scene_view, const glm::uvec2 render_resolution) {
    const auto jitter = scene_view.get_jitter();
    params.jitterOffsetX = -jitter.x;
    params.jitterOffsetY = -jitter.y;
    params.exposureScale = 1;
    params.inputWidth = render_resolution.x;
    params.inputHeight = render_resolution.y;
}

static xess_vk_image_view_info wrap_image(TextureHandle texture);

void XeSSAdapter::evaluate(
    RenderGraph& graph, const SceneView& view, const GBuffer& gbuffer, const TextureHandle color_in,
    const TextureHandle color_out, const TextureHandle motion_vectors_in
) {
    graph.add_pass(
        {
            .name = "xess",
            .textures = {
                {
                    .texture = color_in,
                    .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .access = VK_ACCESS_2_SHADER_READ_BIT,
                    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                },
                {
                    .texture = color_out,
                    .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .access = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
                    .layout = VK_IMAGE_LAYOUT_GENERAL
                },
                {
                    .texture = gbuffer.depth,
                    .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .access = VK_ACCESS_2_SHADER_READ_BIT,
                    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                },
                {
                    .texture = motion_vectors_in,
                    .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .access = VK_ACCESS_2_SHADER_READ_BIT,
                    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                },
            },
            .execute = [&](CommandBuffer& commands) {
                params.colorTexture = wrap_image(color_in);
                params.velocityTexture = wrap_image(motion_vectors_in);
                params.depthTexture = wrap_image(gbuffer.depth);
                params.outputTexture = wrap_image(color_out);

                auto result = xessVKExecute(context, commands.get_vk_commands(), &params);
                if(result != XESS_RESULT_SUCCESS) {
                    logger->error("Could not evaluate XeSS: {}", result);
                }
            }
        });
}

xess_vk_image_view_info wrap_image(const TextureHandle texture) {
    return {
        .imageView = texture->attachment_view,
        .image = texture->image,
        .subresourceRange = {
            .aspectMask = static_cast<VkImageAspectFlags>(is_depth_format(texture->create_info.format)
                ? VK_IMAGE_ASPECT_DEPTH_BIT
                : VK_IMAGE_ASPECT_COLOR_BIT),
            .baseMipLevel = 0, .levelCount = 1,
            .baseArrayLayer = 0, .layerCount = 1
        },
        .format = texture->create_info.format,
        .width = texture->create_info.extent.width,
        .height = texture->create_info.extent.height
    };
}
#endif
