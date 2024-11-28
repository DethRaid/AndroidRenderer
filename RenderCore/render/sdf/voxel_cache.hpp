#pragma once
#include <unordered_map>

#include "model_import/mesh_voxelizer.hpp"
#include "render/sdf/lpv_gv_voxelizer.hpp"
#include "render/mesh_handle.hpp"
#include "render/sdf/voxel_object.hpp"
#include "shared/voxel_object_gpu.hpp"

class RenderBackend;

/**
 * Holds voxel volumes for all the loaded meshes
 */
class VoxelCache {
public:
    explicit VoxelCache(RenderBackend& backend_in);

    ~VoxelCache();

    /**
     * Creates a voxel volume for the given mesh
     */
    VoxelObject build_voxels_for_mesh(MeshPrimitiveHandle primitive, const MeshStorage& meshes, BufferHandle primitive_data_buffer, RenderGraph& graph);

    const VoxelObject& get_voxel_for_primitive(MeshPrimitiveHandle primitive) const;

private:
    RenderBackend& backend;

    // ThreeDeeRasterizer voxelizer;

    MeshVoxelizer voxelizer;

    /**
     * Map from mesh index and material index to voxel
     */
    absl::flat_hash_map<uint64_t, VoxelObject> voxels;

    static uint64_t make_key(MeshPrimitiveHandle primitive);
};
