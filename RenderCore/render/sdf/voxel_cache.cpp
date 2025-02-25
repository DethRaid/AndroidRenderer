#include "voxel_cache.hpp"

#include "console/cvars.hpp"
#include "render/backend/render_backend.hpp"

[[maybe_unused]] static AutoCVar_Int cvar_enable_voxelizer{
    "r.voxel.Enable", "Whether or not to voxelize meshes and use those voxels for various purposes", 0
};

[[maybe_unused]] static AutoCVar_Float cvar_voxel_size{
    "r.voxel.VoxelSize", "Resolution, in world units, of one side of a mesh voxel", 0.25
};

[[maybe_unused]] static AutoCVar_Int cvar_enable_voxel_visualizer{
    "r.voxel.Visualize", "Turns on the visualization of voxels", 0
};

VoxelCache::VoxelCache(RenderBackend& backend_in) : backend{backend_in}, voxelizer{backend} {}

VoxelCache::~VoxelCache() {
    auto& allocator = backend.get_global_allocator();
    for(const auto& [mesh_index, voxel_object] : voxels) {
        allocator.destroy_texture(voxel_object.voxels_color);
        allocator.destroy_texture(voxel_object.voxels_normals);
    }

    voxels.clear();
}

VoxelObject VoxelCache::build_voxels_for_mesh(
    const MeshPrimitiveHandle primitive, const MeshStorage& meshes, const BufferHandle primitive_data_buffer,
    RenderGraph& graph
) {
    const auto key = make_key(primitive);

    if(auto itr = voxels.find(key); itr != voxels.end()) {
        return itr->second;
    }

    const auto voxel_texture = voxelizer.voxelize_primitive(
        graph,
        primitive,
        meshes,
        primitive_data_buffer,
        cvar_voxel_size.GetFloat()
    );

    auto obj = VoxelObject{
        .worldspace_size = voxel_texture.num_voxels,
        .voxels_color = voxel_texture.color_texture,
        .voxels_normals = voxel_texture.normals_texture,
    };
    voxels.emplace(key, obj);

    return obj;
}

const VoxelObject& VoxelCache::get_voxel_for_primitive(const MeshPrimitiveHandle primitive) const {
    return voxels.at(make_key(primitive));
}

uint64_t VoxelCache::make_key(const MeshPrimitiveHandle primitive) {
    uint64_t key = 0;
    key |= primitive->mesh.index;
    key |= static_cast<uint64_t>(primitive->material.index) << 32;

    return key;
}
