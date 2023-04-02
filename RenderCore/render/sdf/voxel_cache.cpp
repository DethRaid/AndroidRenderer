#include "voxel_cache.hpp"

#include <ranges>
#include <glm/common.hpp>

#include "console/cvars.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/render_graph.hpp"

AutoCVar_Float cvar_voxel_size{"r.voxel.VoxelSize", "Resolution, in world units, of one side of a mesh voxel", 0.5};

VoxelCache::VoxelCache(RenderBackend& backend_in) : backend{ backend_in }  {}

VoxelCache::~VoxelCache() {
    auto& allocator = backend.get_global_allocator();
    for(const auto& voxel_object : voxels | std::views::values) {
        allocator.destroy_texture(voxel_object.sh_texture);
    }

    voxels.clear();
}

void VoxelCache::build_voxels_for_mesh(const MeshHandle mesh, MeshStorage& meshes) {
    const auto num_voxels = glm::uvec3{ glm::ceil(mesh->bounds / cvar_voxel_size.GetFloat()) };

    const auto num_triangles = mesh->num_indices / 3;
    voxelizer.init_resources(backend, num_voxels, num_triangles);

    auto graph = RenderGraph{ backend };
    voxelizer.voxelize_mesh(graph, mesh, meshes);
    graph.finish();

    auto obj = VoxelObject{
        .worldspace_size = glm::vec3{num_voxels},
        .sh_texture = voxelizer.extract_texture(),
    };

    voxels.emplace(mesh.index, obj);
}
