#include "streamline_adapter.hpp"

#if SAH_USE_STREAMLINE
#include <sl_core_api.h>
#include <sl_core_types.h>
#include <sl_security.h>

#include "core/system_interface.hpp"

static std::shared_ptr<spdlog::logger> logger;

StreamlineAdapter::StreamlineAdapter() {
    if (logger == nullptr) {
        logger = SystemInterface::get().get_logger("StreamlineAdapter");
    }

    auto features_to_load = eastl::vector<sl::Feature>{sl::kFeatureDLSS, sl::kFeatureDLSS_RR };

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
    if (result != sl::Result::eOk) {
        throw std::runtime_error{ "Could not initialize Streamline" };
    }
}

StreamlineAdapter::~StreamlineAdapter() {
    slShutdown();
}

PFN_vkGetInstanceProcAddr StreamlineAdapter::try_load_interposer() {
    const auto path = std::filesystem::path{ SAH_BINARY_DIR } / "sl.interposer.dll";
    const auto streamline_dir = path.generic_u16string();
    const auto* skill_issue_char = reinterpret_cast<const wchar_t*>(streamline_dir.c_str());
    if (!sl::security::verifyEmbeddedSignature(skill_issue_char)) {
        // SL module not signed, disable SL
        return nullptr;
    }
    else {
        auto mod = LoadLibraryW(skill_issue_char);
        if (mod == nullptr) {
            return nullptr;
        }

        return reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(mod, "vkGetInstanceProcAddr"));
    }
}
#endif