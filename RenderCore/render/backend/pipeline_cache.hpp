#pragma once

#include "graphics_pipeline.hpp"
#include "core/object_pool.hpp"

class RenderBackend;

class PipelineCache {
public:
    explicit PipelineCache(RenderBackend& backend_in);

    ~PipelineCache();

    GraphicsPipelineHandle create_pipeline(const GraphicsPipelineBuilder& pipeline_builder);

    VkPipeline get_pipeline(GraphicsPipelineHandle pipeline, VkRenderPass active_render_pass, uint32_t active_subpass) const;

private:
    RenderBackend& backend;

    VkPipelineCache vk_pipeline_cache = VK_NULL_HANDLE;

    ObjectPool<GraphicsPipeline> pipelines;
};
