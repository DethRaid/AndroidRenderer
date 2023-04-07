#pragma once
#include <unordered_map>

#include "render/sdf/lpv_gv_voxelizer.hpp"
#include "render/mesh_handle.hpp"
#include "render/sdf/voxel_object.hpp"

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
    void build_voxels_for_mesh(MeshHandle mesh, const MeshStorage& meshes);

    const VoxelObject& get_voxel_for_mesh(MeshHandle mesh) const;

private:
    RenderBackend& backend;

    ThreeDeeRasterizer voxelizer;

    /**
     * Map from mesh index to voxel
     */
    std::unordered_map<uint32_t, VoxelObject> voxels;
};
