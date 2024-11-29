#pragma once

#include "render/backend/scatter_upload_buffer.hpp"
#include "render/basic_pbr_material.hpp"
#include "core/object_pool.hpp"
#include "render/material_proxy.hpp"

class RenderGraph;
class RenderBackend;

class MaterialStorage {
public:
    explicit MaterialStorage();

    PooledObject<BasicPbrMaterialProxy> add_material(BasicPbrMaterial&& new_material);

    void destroy_material(PooledObject<BasicPbrMaterialProxy>&& proxy);

    void flush_material_buffer(RenderGraph& graph);

    BufferHandle get_material_buffer() const;

    GraphicsPipelineHandle get_pipeline_group();

private:
    bool is_pipeline_group_dirty = true;
    GraphicsPipelineHandle pipeline_group{};

    ObjectPool<BasicPbrMaterialProxy> material_pool;

    ScatterUploadBuffer<BasicPbrMaterialGpu> material_upload;

    BufferHandle material_buffer_handle;
};
