#include "scatter_upload_buffer.hpp"

#include "render/backend/pipeline_cache.hpp"

static ComputePipelineHandle scatter_shader = nullptr;

ComputePipelineHandle get_scatter_upload_shader() {
    if (!scatter_shader) {
        auto& backend = RenderBackend::get();
        auto& pipeline_cache = backend.get_pipeline_cache();
        scatter_shader = pipeline_cache.create_pipeline("shaders/scatter_upload.comp.spv");
    }

    return scatter_shader;
}