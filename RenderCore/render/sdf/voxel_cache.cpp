#include "voxel_cache.hpp"

#include <glm/common.hpp>

#include "console/cvars.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/render_graph.hpp"

AutoCVar_Int cvar_enable_voxelizer{ "r.voxel.Enable", "Whether or not to voxelize meshes and use those voxels for various purposes", 1};

AutoCVar_Float cvar_voxel_size{"r.voxel.VoxelSize", "Resolution, in world units, of one side of a mesh voxel", 0.25};

VoxelCache::VoxelCache(RenderBackend& backend_in) : backend{ backend_in }, voxelizer{ backend } {}

VoxelCache::~VoxelCache() {
    auto& allocator = backend.get_global_allocator();
    for(const auto& [mesh_index, voxel_object] : voxels) {
        allocator.destroy_texture(voxel_object.voxels);
    }

    voxels.clear();
}

VoxelObject VoxelCache::build_voxels_for_mesh(const MeshPrimitiveHandle primitive, const MeshStorage& meshes, const BufferHandle primitive_data_buffer) {
    const auto key = make_key(primitive);

    if(auto itr = voxels.find(key); itr != voxels.end()) {
        return itr->second;
    }

    const auto mesh = primitive->mesh;
    const auto num_voxels = glm::uvec3{ glm::ceil(mesh->bounds / cvar_voxel_size.GetFloat()) };

    auto graph = RenderGraph{ backend };
    const auto voxel_texture = voxelizer.voxelize_primitive(graph, primitive, meshes, primitive_data_buffer, cvar_voxel_size.GetFloat());
    graph.finish();

    backend.execute_graph(std::move(graph));

    auto obj = VoxelObject{
        .worldspace_size = glm::vec3{num_voxels},
        .voxels = voxel_texture,
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
