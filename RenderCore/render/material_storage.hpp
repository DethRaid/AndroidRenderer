#pragma once

#include "material_pipelines.hpp"
#include "render/backend/scatter_upload_buffer.hpp"
#include "render/basic_pbr_material.hpp"
#include "core/object_pool.hpp"
#include "render/material_proxy.hpp"

class RenderGraph;
class RenderBackend;

class MaterialStorage {
public:
    explicit MaterialStorage();

    PooledObject<BasicPbrMaterialProxy> add_material_instance(BasicPbrMaterial&& new_material);

    void destroy_material_instance(PooledObject<BasicPbrMaterialProxy>&& proxy);

    void flush_material_instance_buffer(RenderGraph& graph);

    BufferHandle get_material_instance_buffer() const;

    GraphicsPipelineHandle get_pipeline_group();

    const MaterialPipelines& get_pipelines() const;

private:
    MaterialPipelines basic_bpr_material_pipelines;

    bool is_pipeline_group_dirty = true;
    GraphicsPipelineHandle pipeline_group = nullptr;

    ObjectPool<BasicPbrMaterialProxy> material_instance_pool;

    ScatterUploadBuffer<BasicPbrMaterialGpu> material_instance_upload_buffer;

    BufferHandle material_instance_buffer_handle;
};
