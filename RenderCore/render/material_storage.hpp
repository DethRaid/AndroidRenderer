#pragma once

#include "render/basic_pbr_material.hpp"
#include "core/object_pool.hpp"
#include "render/material_proxy.hpp"

class CommandBuffer;
class RenderBackend;

class MaterialStorage {
public:
    explicit MaterialStorage(RenderBackend& backend_in);

    PooledObject<BasicPbrMaterialProxy> add_material(BasicPbrMaterial&& new_material);

    void destroy_material(PooledObject<BasicPbrMaterialProxy>&& proxy);

    void flush_material_buffer(CommandBuffer& commands);

private:
    RenderBackend& backend;

    ObjectPool<BasicPbrMaterialProxy> materials;

    BufferHandle material_buffer_handle;
};
