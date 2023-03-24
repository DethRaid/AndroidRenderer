#pragma once

#include "mesh_drawer.hpp"
#include "scene_pass_type.hpp"
#include "core/object_pool.hpp"
#include "render/backend/handles.hpp"
#include "render/scene_primitive.hpp"
#include "render/backend/scatter_upload_buffer.hpp"
#include "render/sun_light.hpp"

class GltfModel;
class RenderBackend;

/**
 * A scene that can be rendered!
 *
 * Contains lots of wonderful things
 */
class RenderScene {
public:
    explicit RenderScene(RenderBackend& backend_in);

    PooledObject<MeshPrimitive> add_primitive(RenderGraph& graph, MeshPrimitive primitive);

    void flush_primitive_upload(RenderGraph& graph);

    void add_model(GltfModel& model);

    const std::vector<PooledObject<MeshPrimitive>>& get_solid_primitives() const;

    BufferHandle get_primitive_buffer() const;

    SunLight &get_sun_light();

    SceneDrawer create_view(ScenePassType type, const MeshStorage& meshes);

private:
    RenderBackend& backend;

    SunLight sun;

    ObjectPool<MeshPrimitive> mesh_primitives;

    BufferHandle primitive_data_buffer;

    ScatterUploadBuffer<PrimitiveData> primitive_upload_buffer;

    std::vector<PooledObject<MeshPrimitive>> solid_primitives;

    std::vector<PooledObject<MeshPrimitive>>  cutout_primitives;

    std::vector<PooledObject<MeshPrimitive>>  translucent_primitives;
};



