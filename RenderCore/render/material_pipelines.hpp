#pragma once

#include <string_view>

#include "backend/handles.hpp"

/**
 * Basic storage for material pipelines
 */
class MaterialPipelines
{
public:
    explicit MaterialPipelines(std::string_view material_name);

    GraphicsPipelineHandle get_depth_pso() const;
    GraphicsPipelineHandle get_depth_masked_pso() const;

    GraphicsPipelineHandle get_shadow_pso() const;
    GraphicsPipelineHandle get_shadow_masked_pso() const;

    GraphicsPipelineHandle get_rsm_pso() const;
    GraphicsPipelineHandle get_rsm_masked_pso() const;

    GraphicsPipelineHandle get_gbuffer_pso() const;
    GraphicsPipelineHandle get_gbuffer_masked_pso() const;

private:
    GraphicsPipelineHandle depth_pso = nullptr;
    GraphicsPipelineHandle depth_masked_pso = nullptr;

    GraphicsPipelineHandle shadow_pso = nullptr;
    GraphicsPipelineHandle shadow_masked_pso = nullptr;

    GraphicsPipelineHandle rsm_pso = nullptr;
    GraphicsPipelineHandle rsm_masked_pso = nullptr;

    GraphicsPipelineHandle gbuffer_pso = nullptr;
    GraphicsPipelineHandle gbuffer_masked_pso = nullptr;
};

