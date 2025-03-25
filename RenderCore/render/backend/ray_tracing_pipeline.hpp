#pragma once

#include <string>
#include <EASTL/vector.h>

#include <vulkan/vulkan_core.h>

#include "render/backend/pipeline_interface.hpp"
#include "render/backend/handles.hpp"

struct HitGroup {
    std::string name;

    /**
     * Index of this hit group in the hit groups array. Necessary for materials to work
     *
     * Note that hit groups have two shader groups - one for occlusion, one for GI
     */
    uint32_t index = 0;

    /**
     * Anyhit shader to use when testing for occlusion. Empty for solid hitgroups
     */
    eastl::vector<std::byte> occlusion_anyhit_shader;

    /**
     * Closesthit shader to use when testing occlusion
     */
    eastl::vector<std::byte> occlusion_closesthit_shader;

    /**
     * Anyhit shader to use when sampling GI. Empty for solid hitgroups
     */
    eastl::vector<std::byte> gi_anyhit_shader;

    /**
     * Closesthit shader to use when sampling GI
     */
    eastl::vector<std::byte> gi_closesthit_shader;
};

struct RayTracingPipeline : PipelineBase
{
    BufferHandle shader_tables_buffer = nullptr;

    VkStridedDeviceAddressRegionKHR raygen_table = {};
    VkStridedDeviceAddressRegionKHR hit_table = {};
    VkStridedDeviceAddressRegionKHR miss_table = {};
};

