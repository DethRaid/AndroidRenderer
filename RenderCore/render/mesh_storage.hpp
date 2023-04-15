#pragma once

#include <span>

#include <vk_mem_alloc.h>
#include <tl/optional.hpp>

#include "render/mesh_handle.hpp"
#include "core/object_pool.hpp"
#include "render/backend/handles.hpp"
#include "render/mesh.hpp"
#include "render/standard_vertex.hpp"
#include "shared/vertex_data.hpp"

class ResourceAllocator;
class ResourceUploadQueue;

struct MeshPoint {
    uint32_t triangle_id;
    glm::vec3 barycentric;
};

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

    VmaVirtualBlock vertex_block;
    BufferHandle vertex_position_buffer;
    BufferHandle vertex_data_buffer;

    VmaVirtualBlock index_block;
    BufferHandle index_buffer;

    std::pair<std::vector<MeshPoint>, float> generate_surface_point_cloud(std::span<const StandardVertex> vertices, std::span<const uint32_t> indices);
};
