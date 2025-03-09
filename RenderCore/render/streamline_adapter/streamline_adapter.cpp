#include "streamline_adapter.hpp"

#if SAH_USE_STREAMLINE

#include <stdexcept>

#include <sl_security.h>
#include <sl_helpers.h>
#include <sl_helpers_vk.h>
#include <sl_matrix_helpers.h>
#include <libloaderapi.h>
#include <utf8.h>

#include "core/string_conversion.hpp"
#include "core/system_interface.hpp"

static std::shared_ptr<spdlog::logger> logger;

static sl::Resource wrap_resource(TextureHandle texture, VkImageLayout layout);

PFN_vkGetInstanceProcAddr StreamlineAdapter::try_load_streamline() {
    const auto path = std::filesystem::path{ SAH_BINARY_DIR } / "sl.interposer.dll";
    const auto streamline_dir = path.generic_u16string();
    const auto* skill_issue_char = reinterpret_cast<const wchar_t*>(streamline_dir.c_str());
    if(!sl::security::verifyEmbeddedSignature(skill_issue_char)) {
        // SL module not signed, disable SL
        return nullptr;
    } else {
        auto mod = LoadLibraryW(skill_issue_char);
        if(mod == nullptr) {
            return nullptr;
        }

        available = true;

        return reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(mod, "vkGetInstanceProcAddr"));
    }
}

bool StreamlineAdapter::is_available() {
    return available;
}

StreamlineAdapter::StreamlineAdapter() {
    logger = SystemInterface::get().get_logger("StreamlineAdapter");

    auto features_to_load = std::vector<sl::Feature>{sl::kFeatureDLSS};

    auto prefs = sl::Preferences{};
    prefs.showConsole = true;
    prefs.logLevel = sl::LogLevel::eDefault;
    prefs.flags = sl::PreferenceFlags::eDisableCLStateTracking | sl::PreferenceFlags::eAllowOTA |
        sl::PreferenceFlags::eLoadDownloadedPlugins;
    prefs.featuresToLoad = features_to_load.data();
    prefs.numFeaturesToLoad = static_cast<uint32_t>(features_to_load.size());
    prefs.renderAPI = sl::RenderAPI::eVulkan;
    prefs.engineVersion = "0.10.0";
    prefs.projectId = "450D193B-267E-4755-8C21-592C7FA8A3D4";
    const auto result = slInit(prefs);
    if(result != sl::Result::eOk) {
        throw std::runtime_error{"Could not initialize Streamline"};
    }

    dlss_support = slIsFeatureSupported(sl::kFeatureDLSS, {});
    if(dlss_support == sl::Result::eOk) {
        logger->info("DLSS is supported!");
    } else {
        logger->warn("DLSS is not supported! {}", sl::getResultAsStr(dlss_support));
    }
}

StreamlineAdapter::~StreamlineAdapter() {
    slShutdown();
}

void StreamlineAdapter::set_devices_from_backend(const RenderBackend& backend) {
    auto vk_info = sl::VulkanInfo{};
    vk_info.device = backend.get_device();
    vk_info.instance = backend.get_instance();
    vk_info.physicalDevice = backend.get_physical_device();
    vk_info.computeQueueFamily = backend.get_graphics_queue_family_index();
    vk_info.graphicsQueueFamily = backend.get_graphics_queue_family_index();

    slSetVulkanInfo(vk_info);
}

bool StreamlineAdapter::is_dlss_supported() const {
    return dlss_support == sl::Result::eOk;
}

void StreamlineAdapter::update_frame_token(const uint32_t frame_index) {
    slGetNewFrameToken(frame_token, &frame_index);
}

void StreamlineAdapter::set_constants(const SceneView& scene_transform, const glm::uvec2 render_resolution) {
    const auto& view_data = scene_transform.get_gpu_data();

    auto constants = sl::Constants{};

    const auto jitter = scene_transform.get_jitter();

    auto projection = scene_transform.get_projection();

    const auto inverse_projection = glm::inverse(projection);

    std::memcpy(&constants.cameraViewToClip, &projection, sizeof(glm::mat4));
    std::memcpy(&constants.clipToCameraView, &inverse_projection, sizeof(glm::mat4));

    const auto clip_to_prev_clip = inverse_projection * view_data.inverse_view * view_data.last_frame_view *
        scene_transform.get_last_frame_projection();
    std::memcpy(&constants.clipToPrevClip, &clip_to_prev_clip, sizeof(glm::mat4));

    const auto prev_clip_to_clip = glm::inverse(clip_to_prev_clip);
    std::memcpy(&constants.prevClipToClip, &prev_clip_to_clip, sizeof(glm::mat4));

    const auto scaled_jitter = jitter * glm::vec2{render_resolution};
    constants.jitterOffset = {-scaled_jitter.x, -scaled_jitter.y};

    constants.mvecScale = {1.f / render_resolution.x, 1.f / render_resolution.y};
    
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

    constants.depthInverted = sl::Boolean::eFalse;
    constants.cameraMotionIncluded = sl::Boolean::eTrue;
    constants.motionVectors3D = sl::Boolean::eFalse;
    constants.reset = sl::Boolean::eFalse;
    constants.orthographicProjection = sl::Boolean::eFalse;
    constants.motionVectorsJittered = sl::Boolean::eTrue;

    slSetConstants(constants, *frame_token, viewport);
}

void StreamlineAdapter::set_dlss_mode(const sl::DLSSMode mode) {
    dlss_mode = mode;
    if(dlss_mode != sl::DLSSMode::eOff) {
        bool dlss_loaded = false;
        slIsFeatureLoaded(sl::kFeatureDLSS, dlss_loaded);
        if(!dlss_loaded) {
            auto result = slSetFeatureLoaded(sl::kFeatureDLSS, true);
            if(result != sl::Result::eOk) {
                logger->error("Error loading DLSS: {}", sl::getResultAsStr(result));
            }
        }
    }
}

glm::uvec2 StreamlineAdapter::get_dlss_render_resolution(const glm::uvec2& output_resolution) {
    sl::DLSSOptions dlss_options;
    dlss_options.mode = dlss_mode;
    dlss_options.outputWidth = output_resolution.x;
    dlss_options.outputHeight = output_resolution.y;

    auto result = slDLSSGetOptimalSettings(dlss_options, dlss_settings);
    if(result != sl::Result::eOk) {
        logger->error("Error getting DLSS settings: {}", sl::getResultAsStr(result));
        return output_resolution;
    }

    return glm::uvec2{dlss_settings.optimalRenderWidth, dlss_settings.optimalRenderHeight};
}

void StreamlineAdapter::evaluate_dlss(
    CommandBuffer& commands,
    const TextureHandle color_in, const TextureHandle color_out,
    const TextureHandle depth_in, const TextureHandle motion_vectors_in
) {
    auto color_in_res = wrap_resource(color_in, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    auto color_out_res = wrap_resource(color_out, VK_IMAGE_LAYOUT_GENERAL);
    auto depth_in_res = wrap_resource(depth_in, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    auto motion_vectors_in_res = wrap_resource(motion_vectors_in, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    auto color_in_tag = sl::ResourceTag{
        &color_in_res, sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eValidUntilPresent
    };
    auto color_out_tag = sl::ResourceTag{
        &color_out_res, sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eValidUntilPresent
    };
    auto depth_tag = sl::ResourceTag{&depth_in_res, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent};
    auto motion_vectors_tag = sl::ResourceTag{
        &motion_vectors_in_res, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilPresent
    };

    auto tags = std::array{color_in_tag, color_out_tag, depth_tag, motion_vectors_tag};
    slSetTag(viewport, tags.data(), tags.size(), commands.get_vk_commands());

    auto options = sl::DLSSOptions{};
    options.mode = dlss_mode;
    const auto& output_resolution = color_out->create_info.extent;
    options.outputWidth = output_resolution.width;
    options.outputHeight = output_resolution.height;
    options.sharpness = dlss_settings.optimalSharpness;
    options.useAutoExposure = sl::Boolean::eTrue;
    slDLSSSetOptions(viewport, options);

    auto options_arr = std::array<const sl::BaseStructure*, 1>{&viewport};
    const auto result = slEvaluateFeature(
        sl::kFeatureDLSS,
        *frame_token,
        options_arr.data(),
        options_arr.size(),
        commands.get_vk_commands());
    if(result != sl::Result::eOk) {
        logger->error("Error evaluating DLSS: {}", sl::getResultAsStr(result));
    }
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
