#pragma once

#include <span>

#include <volk.h>
#include <vk_mem_alloc.h>
#include <tl/optional.hpp>

#include "render/backend/scatter_upload_buffer.hpp"
#include "render/mesh_handle.hpp"
#include "core/object_pool.hpp"
#include "render/backend/handles.hpp"
#include "render/mesh.hpp"
#include "shared/vertex_data.hpp"
#include "shared/mesh_point.hpp"

class RenderBackend;
class ResourceUploadQueue;

/**
 * Stores meshes
 */
class MeshStorage {
public:
    explicit MeshStorage();

    ~MeshStorage();

    tl::optional<MeshHandle> add_mesh(
        std::span<const StandardVertex> vertices, std::span<const uint32_t> indices, const Box& bounds
    );

    void free_mesh(MeshHandle mesh);

    void flush_mesh_draw_arg_uploads(RenderGraph& graph);

    BufferHandle get_vertex_position_buffer() const;

    BufferHandle get_vertex_data_buffer() const;

    BufferHandle get_index_buffer() const;

    BufferHandle get_draw_args_buffer() const;

private:
    ObjectPool<Mesh> meshes;

    ScatterUploadBuffer<VkDrawIndexedIndirectCommand> mesh_draw_args_upload_buffer;
    BufferHandle mesh_draw_args_buffer = {};

    // vertex_block and index_block measure vertices and indices, respectively

    VmaVirtualBlock vertex_block = {};
    BufferHandle vertex_position_buffer = {};
    BufferHandle vertex_data_buffer = {};

    VmaVirtualBlock index_block = {};
    BufferHandle index_buffer = {};

    std::pair<std::vector<StandardVertex>, float> generate_surface_point_cloud(
        std::span<const StandardVertex> vertices, std::span<const uint32_t> indices
    ) const;

    static StandardVertex interpolate_vertex(std::span<const StandardVertex> vertices, std::span<const uint32_t> indices, size_t triangle_id, glm::vec3 barycentric);

    BufferHandle generate_sh_point_cloud(const std::vector<StandardVertex>& point_cloud) const;

    void create_blas_for_mesh(uint32_t first_vertex, uint32_t num_vertices, uint32_t first_index, uint num_triangles) const;
};
