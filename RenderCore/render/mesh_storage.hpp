#pragma once

#include <span>

#include <vk_mem_alloc.h>
#include <tl/optional.hpp>

#include "render/mesh_handle.hpp"
#include "core/object_pool.hpp"
#include "render/backend/handles.hpp"
#include "render/mesh.hpp"
#include "shared/vertex_data.hpp"
#include "shared/mesh_point.hpp"

class ResourceAllocator;
class ResourceUploadQueue;

/**
 * Stores meshes
 */
class MeshStorage {
public:
    explicit MeshStorage(ResourceAllocator& allocator_in, ResourceUploadQueue& queue_in);

    ~MeshStorage();

    tl::optional<MeshHandle> add_mesh(
        std::span<const StandardVertex> vertices, std::span<const uint32_t> indices, const glm::vec3& bounds
    );

    void free_mesh(MeshHandle mesh);

    BufferHandle get_vertex_position_buffer() const;

    BufferHandle get_vertex_data_buffer() const;

    BufferHandle get_index_buffer() const;

private:
    ResourceAllocator* allocator;
    ResourceUploadQueue* upload_queue;

    ObjectPool<Mesh> meshes;

    // vertex_block and index_block measure vertices and indices, respectively

    VmaVirtualBlock vertex_block = {};
    BufferHandle vertex_position_buffer = BufferHandle::None;
    BufferHandle vertex_data_buffer = BufferHandle::None;

    VmaVirtualBlock index_block = {};
    BufferHandle index_buffer = BufferHandle::None;

    std::pair<std::vector<StandardVertex>, float> generate_surface_point_cloud(
        std::span<const StandardVertex> vertices, std::span<const uint32_t> indices
    ) const;

    static StandardVertex interpolate_vertex(std::span<const StandardVertex> vertices, std::span<const uint32_t> indices, size_t triangle_id, glm::vec3 barycentric);

    BufferHandle generate_sh_point_cloud(const std::vector<StandardVertex>& point_cloud) const;
};
