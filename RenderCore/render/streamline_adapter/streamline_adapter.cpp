#include "streamline_adapter.hpp"

#include <sl.h>
#include <sl_consts.h>

#include <sl_security.h>
#include <libloaderapi.h>
#include <stdexcept>

#include "core/string_conversion.hpp"
#include "core/system_interface.hpp"

static std::shared_ptr<spdlog::logger> logger;

PFN_vkGetInstanceProcAddr StreamlineAdapter::try_load_streamline() {
    const auto streamline_dir = to_wstring(SAH_BINARY_DIR) + std::wstring{L"/sl.interposer.dll"};
    if(!sl::security::verifyEmbeddedSignature(streamline_dir.c_str())) {
        // SL module not signed, disable SL
        return nullptr;
    } else {
        auto mod = LoadLibrary(streamline_dir.c_str());
        available = true;

        // Map functions from SL and use them instead of standard DXGI/D3D12 API
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
    prefs.featuresToLoad = features_to_load.data();
    prefs.numFeaturesToLoad = static_cast<uint32_t>(features_to_load.size());
    prefs.renderAPI = sl::RenderAPI::eVulkan;
    const auto result = slInit(prefs);
    if(result != sl::Result::eOk) {
        throw std::runtime_error{"Could not initialize Streamline"};
    }

    dlss_support = slIsFeatureSupported(sl::kFeatureDLSS, {});
    if(dlss_support == sl::Result::eOk) {
        logger->info("DLSS is supported!");
    } else {
        logger->warn("DLSS is not supported! {}", to_string(dlss_support));
    }
}

bool StreamlineAdapter::is_dlss_supported() const {
    return dlss_support == sl::Result::eOk;
}

void StreamlineAdapter::update_frame_token(const uint32_t frame_index) {
    slGetNewFrameToken(frame_token, &frame_index);
}

StreamlineAdapter::~StreamlineAdapter() {
    slShutdown();
}

std::string to_string(const sl::Result result) {
    switch(result) {
    case sl::Result::eOk:
        return "Ok";
    case sl::Result::eErrorIO:
        return "ErrorIO";
    case sl::Result::eErrorDriverOutOfDate:
        return "ErrorDriverOutOfDate";
    case sl::Result::eErrorOSOutOfDate:
        return "ErrorOSOutOfDate";
    case sl::Result::eErrorOSDisabledHWS:
        return "ErrorOSDisabledHWS";
    case sl::Result::eErrorDeviceNotCreated:
        return "ErrorDeviceNotCreated";
    case sl::Result::eErrorNoSupportedAdapterFound:
        return "ErrorNoSupportedAdapterFound";
    case sl::Result::eErrorAdapterNotSupported:
        return "ErrorAdapterNotSupported";
    case sl::Result::eErrorNoPlugins:
        return "ErrorNoPlugins";
    case sl::Result::eErrorVulkanAPI:
        return "ErrorVulkanAPI";
    case sl::Result::eErrorDXGIAPI:
        return "ErrorDXGIAPI";
    case sl::Result::eErrorD3DAPI:
        return "ErrorD3DAPI";
    case sl::Result::eErrorNRDAPI:
        return "ErrorNRDAPI";
    case sl::Result::eErrorNVAPI:
        return "ErrorNVAPI";
    case sl::Result::eErrorReflexAPI:
        return "ErrorReflexAPI";
    case sl::Result::eErrorNGXFailed:
        return "ErrorNGXFailed";
    case sl::Result::eErrorJSONParsing:
        return "ErrorJSONParsing";
    case sl::Result::eErrorMissingProxy:
        return "ErrorMissingProxy";
    case sl::Result::eErrorMissingResourceState:
        return "ErrorMissingResourceState";
    case sl::Result::eErrorInvalidIntegration:
        return "ErrorInvalidIntegration";
    case sl::Result::eErrorMissingInputParameter:
        return "ErrorMissingInputParameter";
    case sl::Result::eErrorNotInitialized:
        return "ErrorNotInitialized";
    case sl::Result::eErrorComputeFailed:
        return "ErrorComputeFailed";
    case sl::Result::eErrorInitNotCalled:
        return "ErrorInitNotCalled";
    case sl::Result::eErrorExceptionHandler:
        return "ErrorExceptionHandler";
    case sl::Result::eErrorInvalidParameter:
        return "ErrorInvalidParameter";
    case sl::Result::eErrorMissingConstants:
        return "ErrorMissingConstants";
    case sl::Result::eErrorDuplicatedConstants:
        return "ErrorDuplicatedConstants";
    case sl::Result::eErrorMissingOrInvalidAPI:
        return "ErrorMissingOrInvalidAPI";
    case sl::Result::eErrorCommonConstantsMissing:
        return "ErrorCommonConstantsMissing";
    case sl::Result::eErrorUnsupportedInterface:
        return "ErrorUnsupportedInterface";
    case sl::Result::eErrorFeatureMissing:
        return "ErrorFeatureMissing";
    case sl::Result::eErrorFeatureNotSupported:
        return "ErrorFeatureNotSupported";
    case sl::Result::eErrorFeatureMissingHooks:
        return "ErrorFeatureMissingHooks";
    case sl::Result::eErrorFeatureFailedToLoad:
        return "ErrorFeatureFailedToLoad";
    case sl::Result::eErrorFeatureWrongPriority:
        return "ErrorFeatureWrongPriority";
    case sl::Result::eErrorFeatureMissingDependency:
        return "ErrorFeatureMissingDependency";
    case sl::Result::eErrorFeatureManagerInvalidState:
        return "ErrorFeatureManagerInvalidState";
    case sl::Result::eErrorInvalidState:
        return "ErrorInvalidState";
    case sl::Result::eWarnOutOfVRAM:
        return "WarnOutOfVRAM";
    default:
        return "unknown error";
    }
}
