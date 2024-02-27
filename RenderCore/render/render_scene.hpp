#pragma once

#include "render/raytracing_scene.hpp"
#include "core/object_pool.hpp"
#include "render/backend/handles.hpp"
#include "render/scene_primitive.hpp"
#include "render/backend/scatter_upload_buffer.hpp"
#include "render/sun_light.hpp"
#include "sdf/voxel_cache.hpp"

class MaterialStorage;
class MeshStorage;
class GltfModel;
class RenderBackend;

/**
 * A scene that can be rendered!
 *
 * Contains lots of wonderful things - meshes, materials, ray tracing acceleration structure, emissive point clouds, voxelized meshes, and more!
 */
class RenderScene {
public:
    explicit RenderScene(RenderBackend& backend_in, MeshStorage& meshes_in, MaterialStorage& materials_in);

    MeshPrimitiveHandle add_primitive(RenderGraph& graph, MeshPrimitive primitive);

    void flush_primitive_upload(RenderGraph& graph);
    
    const std::vector<MeshPrimitiveHandle>& get_solid_primitives() const;

    BufferHandle get_primitive_buffer() const;

    uint32_t get_total_num_primitives() const;

    SunLight &get_sun_light();

    /**
     * Retrieves a list of all solid primitives that lie within the given bounds
     */
    std::vector<MeshPrimitiveHandle> get_primitives_in_bounds(const glm::vec3& min_bounds, const glm::vec3& max_bounds) const;

    /**
     * \brief Generates emissive point clouds for new emissive meshes
     */
    void generate_emissive_point_clouds(RenderGraph& render_graph);

    const MeshStorage& get_meshes() const;

    RaytracingScene& get_raytracing_scene();

    VoxelCache& get_voxel_cache() const;

private:
    RenderBackend& backend;

    MeshStorage& meshes;

    MaterialStorage& materials;

    tl::optional<RaytracingScene> raytracing_scene;

    /**
     * Cache of voxel representations of static meshes
     */
    std::unique_ptr<VoxelCache> voxel_cache = nullptr;

    SunLight sun;

    ObjectPool<MeshPrimitive> mesh_primitives;

    uint32_t total_num_primitives = 0u;
    BufferHandle primitive_data_buffer;

    ScatterUploadBuffer<PrimitiveDataGPU> primitive_upload_buffer;

    std::vector<MeshPrimitiveHandle> solid_primitives;

    std::vector<MeshPrimitiveHandle>  cutout_primitives;

    std::vector<MeshPrimitiveHandle>  translucent_primitives;

    std::vector<MeshPrimitiveHandle> new_emissive_objects;

    ComputePipelineHandle emissive_point_cloud_shader;

    BufferHandle generate_vpls_for_primitive(RenderGraph& graph, const MeshPrimitiveHandle& primitive);
};
