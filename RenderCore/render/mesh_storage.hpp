#pragma once

#include <span>

#include <vk_mem_alloc.h>
#include <tl/optional.hpp>

#include "render/backend/handles.hpp"
#include "render/mesh.hpp"
#include "render/standard_vertex.hpp"

class ResourceAllocator;
class ResourceUploadQueue;

/**
 * Stores meshes
 */
class MeshStorage {
public:
    explicit MeshStorage(ResourceAllocator& allocator_in, ResourceUploadQueue& queue_in);

    ~MeshStorage();

    tl::optional<Mesh> add_mesh(std::span<const StandardVertex> vertices, std::span<const uint32_t> indices);

    void free_mesh(Mesh mesh);

    BufferHandle get_vertex_position_buffer() const;

    BufferHandle get_vertex_data_buffer() const;

    BufferHandle get_index_buffer() const;

private:
    ResourceAllocator* allocator;
    ResourceUploadQueue* upload_queue;

    // vertex_block and index_block measure vertices and indices, respectively

    VmaVirtualBlock vertex_block;
    BufferHandle vertex_position_buffer;
    BufferHandle vertex_data_buffer;

    VmaVirtualBlock index_block;
    BufferHandle index_buffer;
};
