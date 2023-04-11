#pragma once

#include "render/backend/scatter_upload_buffer.hpp"
#include "render/basic_pbr_material.hpp"
#include "core/object_pool.hpp"
#include "render/material_proxy.hpp"

class RenderGraph;
class RenderBackend;

class MaterialStorage {
public:
    explicit MaterialStorage(RenderBackend& backend_in);

    PooledObject<BasicPbrMaterialProxy> add_material(BasicPbrMaterial&& new_material);

    void destroy_material(PooledObject<BasicPbrMaterialProxy>&& proxy);

    void flush_material_buffer(RenderGraph& graph);

    BufferHandle get_material_buffer() const;

private:
    RenderBackend& backend;

    ObjectPool<BasicPbrMaterialProxy> material_pool;

    ScatterUploadBuffer<BasicPbrMaterialGpu> material_upload;

    BufferHandle material_buffer_handle;
};
