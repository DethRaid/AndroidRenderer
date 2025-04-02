#pragma once

#include <EASTL/array.h>
#include <filesystem>
#include <string>
#include <EASTL/vector.h>

#include <volk.h>

#include "render/backend/handles.hpp"

class PipelineCache;

class HitGroupBuilder
{
    friend class PipelineCache;

public:
    HitGroupBuilder(PipelineCache& cache_in);

    HitGroupBuilder& set_name(std::string_view name_in);

    HitGroupBuilder& add_occlusion_closesthit_shader(const std::filesystem::path& shader_path);

    HitGroupBuilder& add_occlusion_anyhit_shader(const std::filesystem::path& shader_path);

    HitGroupBuilder& add_gi_closesthit_shader(const std::filesystem::path& shader_path);

    HitGroupBuilder& add_gi_anyhit_shader(const std::filesystem::path& shader_path);

    HitGroupHandle build();

private:
    PipelineCache& cache;

    std::string name;

    eastl::vector<VkPipelineShaderStageCreateInfo> stages;

    /**
     * First group is for occlusion, second is for GI
     */
    eastl::array<VkRayTracingShaderGroupCreateInfoKHR, 2> groups;

    eastl::vector<std::byte> occlusion_closesthit_shader;
    eastl::vector<std::byte> occlusion_anyhit_shader;
    eastl::vector<std::byte> gi_closesthit_shader;
    eastl::vector<std::byte> gi_anyhit_shader;
};

