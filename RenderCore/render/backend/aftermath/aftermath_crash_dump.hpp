#pragma once

#if defined(_WIN32)
#include <GFSDK_Aftermath_GpuCrashDump.h>
#include <GFSDK_Aftermath_Defines.h>

class AftermathCrashDump {
public:
    static void GFSDK_AFTERMATH_CALL on_gpu_crash_dump(
        const void* gpu_crash_dump, const uint32_t gpu_crash_dump_size, void* user_data
    );
    static void GFSDK_AFTERMATH_CALL on_shader_debug_info(
        const void* shader_debug_info, const uint32_t shader_debug_info_size, void* user_data
    );
    static void GFSDK_AFTERMATH_CALL on_gpu_crash_dump_description(
        PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription add_value, void* user_data
    );
    static void GFSDK_AFTERMATH_CALL on_resolve_marker(
        const void* marker, void* user_data, void** resolved_marker_data, uint32_t* marker_size
    );
};
#endif
