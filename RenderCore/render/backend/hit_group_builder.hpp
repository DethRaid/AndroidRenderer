#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

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

    std::vector<VkPipelineShaderStageCreateInfo> stages;

    /**
     * First group is for occlusion, second is for GI
     */
    std::array<VkRayTracingShaderGroupCreateInfoKHR, 2> groups;

    std::vector<uint8_t> occlusion_closesthit_shader;
    std::vector<uint8_t> occlusion_anyhit_shader;
    std::vector<uint8_t> gi_closesthit_shader;
    std::vector<uint8_t> gi_anyhit_shader;
};

