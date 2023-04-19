#pragma once

#include "scene_renderer.hpp"
#include "core/object_pool.hpp"
#include "render/backend/handles.hpp"
#include "render/scene_primitive.hpp"
#include "render/backend/scatter_upload_buffer.hpp"
#include "render/sun_light.hpp"

class MaterialStorage;
class MeshStorage;
class GltfModel;
class RenderBackend;

/**
 * A scene that can be rendered!
 *
 * Contains lots of wonderful things
 */
class RenderScene {
public:
    explicit RenderScene(RenderBackend& backend_in, MeshStorage& meshes_in, MaterialStorage& materials_in);

    PooledObject<MeshPrimitive> add_primitive(RenderGraph& graph, MeshPrimitive primitive);

    void flush_primitive_upload(RenderGraph& graph);
    
    const std::vector<PooledObject<MeshPrimitive>>& get_solid_primitives() const;

    BufferHandle get_primitive_buffer() const;

    SunLight &get_sun_light();

    /**
     * Retrieves a list of all solid primitives that lie within the given bounds
     */
    std::vector<PooledObject<MeshPrimitive>> get_primitives_in_bounds(const glm::vec3& min_bounds, const glm::vec3& max_bounds) const;

    /**
     * \brief Generates emissive point clouds for new emissive meshes
     */
    void generate_emissive_point_clouds(RenderGraph& render_graph);

private:
    RenderBackend& backend;

    MeshStorage& meshes;

    MaterialStorage& materials;

    SunLight sun;

    ObjectPool<MeshPrimitive> mesh_primitives;

    BufferHandle primitive_data_buffer;

    ScatterUploadBuffer<PrimitiveDataGPU> primitive_upload_buffer;

    std::vector<PooledObject<MeshPrimitive>> solid_primitives;

    std::vector<PooledObject<MeshPrimitive>>  cutout_primitives;

    std::vector<PooledObject<MeshPrimitive>>  translucent_primitives;

    std::vector<PooledObject<MeshPrimitive>> new_emissive_objects;

    ComputeShader emissive_point_cloud_shader;

    BufferHandle generate_vpls_for_primitive(RenderGraph& graph, const PooledObject<MeshPrimitive>& primitive);
};
