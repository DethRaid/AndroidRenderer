#pragma once

#include <span>

#include <plf_colony.h>

#include "render/backend/compute_shader.hpp"
#include "render/backend/graphics_pipeline.hpp"
#include "render/backend/pipeline_builder.hpp"

class RenderBackend;

class PipelineCache {
public:
    explicit PipelineCache(RenderBackend& backend_in);

    ~PipelineCache();

    GraphicsPipelineHandle create_pipeline(const GraphicsPipelineBuilder& pipeline_builder);

    ComputePipelineHandle create_pipeline(const std::string& shader_file_path);

    GraphicsPipelineHandle create_pipeline_group(std::span<GraphicsPipelineHandle> pipelines_in);

    VkPipeline get_pipeline_for_dynamic_rendering(
        GraphicsPipelineHandle pipeline,
        std::span<const VkFormat> color_attachment_formats,
        std::optional<VkFormat> depth_format = std::nullopt,
        uint32_t view_mask = 0xFF,
        bool use_fragment_shading_rate_attachment = false
    ) const;

    VkPipeline get_pipeline(
        GraphicsPipelineHandle pipeline, VkRenderPass active_render_pass, uint32_t active_subpass
    ) const;

private:
    RenderBackend& backend;

    VkPipelineCache vk_pipeline_cache = VK_NULL_HANDLE;

    plf::colony<GraphicsPipeline> pipelines;

    plf::colony<ComputeShader> compute_pipelines;
};
