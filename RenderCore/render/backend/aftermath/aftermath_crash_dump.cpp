#include "aftermath_crash_dump.hpp"

#include "core/system_interface.hpp"

#if defined(_WIN32)

static std::mutex mutie;

void AftermathCrashDump::on_gpu_crash_dump(
    const void* gpu_crash_dump, const uint32_t gpu_crash_dump_size, void* user_data
) {
    std::lock_guard lock{ mutie };

    SystemInterface::get().write_file("GpuCrashDump.bin", gpu_crash_dump, gpu_crash_dump_size);
}

void AftermathCrashDump::on_shader_debug_info(
    const void* shader_debug_info, const uint32_t shader_debug_info_size, void* user_data
) {
    std::lock_guard lock{ mutie };
}

void AftermathCrashDump::on_gpu_crash_dump_description(
    const PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription add_value, void* user_data
) {
    add_value(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, "SAH Renderer");
    add_value(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationVersion, "0.3.0");
}

void AftermathCrashDump::on_resolve_marker(
    const void* marker, void* user_data, void** resolved_marker_data, uint32_t* marker_size
) {}
#endif
