#pragma once

#include "backend/handles.hpp"

class RenderBackend;
/**
 * Basic storage for material pipelines
 *
 * We only support the standard glTF PBR material, nothing more. This lets us make a lot of assumptions, which enables
 * this class
 */
class MaterialPipelines
{
public:
    static MaterialPipelines& get();

    MaterialPipelines();

    GraphicsPipelineHandle get_depth_pso() const;
    GraphicsPipelineHandle get_depth_masked_pso() const;

    GraphicsPipelineHandle get_shadow_pso() const;
    GraphicsPipelineHandle get_shadow_masked_pso() const;

    GraphicsPipelineHandle get_rsm_pso() const;
    GraphicsPipelineHandle get_rsm_masked_pso() const;

    GraphicsPipelineHandle get_gbuffers_pso() const;

    GraphicsPipelineHandle get_transparent_pso() const;

private:
    GraphicsPipelineHandle depth_pso = nullptr;
    GraphicsPipelineHandle depth_masked_pso = nullptr;

    GraphicsPipelineHandle shadow_pso = nullptr;
    GraphicsPipelineHandle shadow_masked_pso = nullptr;

    GraphicsPipelineHandle rsm_pso = nullptr;
    GraphicsPipelineHandle rsm_masked_pso = nullptr;

    GraphicsPipelineHandle gbuffers_pso = nullptr;

    GraphicsPipelineHandle transparent_pso = nullptr;
};

