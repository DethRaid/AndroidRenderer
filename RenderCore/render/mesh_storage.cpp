#include "mesh_storage.hpp"

#include "render/backend/resource_allocator.hpp"
#include "render/backend/resource_upload_queue.hpp"
#include "shared/vertex_data.hpp"

constexpr const uint32_t max_num_vertices = 100000000;
constexpr const uint32_t max_num_indices = 100000000;

MeshStorage::MeshStorage(ResourceAllocator& allocator_in, ResourceUploadQueue& queue_in) :
    allocator{&allocator_in}, upload_queue{&queue_in} {
    vertex_position_buffer = allocator_in.create_buffer(
        "Vertex position buffer",
        max_num_vertices * sizeof(VertexPosition),
        BufferUsage::VertexBuffer
    );
    vertex_data_buffer = allocator_in.create_buffer(
        "Vertex data buffer",
        max_num_vertices * sizeof(StandardVertexData),
        BufferUsage::VertexBuffer
    );
    index_buffer = allocator_in.create_buffer(
        "Index buffer", max_num_indices * sizeof(uint32_t),
        BufferUsage::IndexBuffer
    );

    const auto vertex_block_create_info = VmaVirtualBlockCreateInfo{
        .size = max_num_vertices,
    };
    vmaCreateVirtualBlock(&vertex_block_create_info, &vertex_block);

    const auto index_block_create_info = VmaVirtualBlockCreateInfo{
        .size = max_num_indices,
    };
    vmaCreateVirtualBlock(&index_block_create_info, &index_block);
}

MeshStorage::~MeshStorage() {
    allocator->destroy_buffer(vertex_position_buffer);
    allocator->destroy_buffer(vertex_data_buffer);
    allocator->destroy_buffer(index_buffer);

    // Yeet all the meshes, even if not explicitly destroyed
    vmaClearVirtualBlock(vertex_block);
    vmaClearVirtualBlock(index_block);
    vmaDestroyVirtualBlock(vertex_block);
    vmaDestroyVirtualBlock(index_block);
}

tl::optional<MeshHandle> MeshStorage::add_mesh(
    const std::span<const StandardVertex> vertices, const std::span<const uint32_t> indices, const glm::vec3& bounds
) {
    auto mesh = Mesh{};

    const auto vertex_allocate_info = VmaVirtualAllocationCreateInfo{
        .size = vertices.size(),
    };
    auto result = vmaVirtualAllocate(vertex_block, &vertex_allocate_info, &mesh.vertex_allocation, &mesh.first_vertex);
    if (result != VK_SUCCESS) {
        return tl::nullopt;
    }

    const auto index_allocate_info = VmaVirtualAllocationCreateInfo{
        .size = indices.size(),
    };
    result = vmaVirtualAllocate(index_block, &index_allocate_info, &mesh.index_allocation, &mesh.first_index);
    if (result != VK_SUCCESS) {
        vmaVirtualFree(vertex_block, mesh.vertex_allocation);
        return tl::nullopt;
    }

    mesh.num_vertices = static_cast<uint32_t>(vertices.size());
    mesh.num_indices = static_cast<uint32_t>(indices.size());
    mesh.bounds = bounds;

    auto positions = std::vector<VertexPosition>{};
    auto data = std::vector<StandardVertexData>{};
    positions.reserve(vertices.size());
    data.reserve(vertices.size());

    for (const auto& vertex : vertices) {
        positions.push_back(vertex.position);
        data.push_back(
            StandardVertexData{
                .normal = vertex.normal,
                .tangent = vertex.tangent,
                .texcoord = vertex.texcoord,
                .color = vertex.color,
            }
        );
    }

    upload_queue->upload_to_buffer<VertexPosition>(
        vertex_position_buffer, positions,
        static_cast<uint32_t>(mesh.first_vertex * sizeof(VertexPosition))
    );
    upload_queue->upload_to_buffer<StandardVertexData>(
        vertex_data_buffer, data,
        static_cast<uint32_t>(mesh.first_vertex *
            sizeof(StandardVertexData))
    );
    upload_queue->upload_to_buffer(index_buffer, indices, static_cast<uint32_t>(mesh.first_index * sizeof(uint32_t)));

    return meshes.add_object(std::move(mesh));
}

void MeshStorage::free_mesh(const MeshHandle mesh) {
    vmaVirtualFree(vertex_block, mesh->vertex_allocation);
    vmaVirtualFree(index_block, mesh->index_allocation);

    meshes.free_object(mesh);
}

BufferHandle MeshStorage::get_vertex_position_buffer() const {
    return vertex_position_buffer;
}

BufferHandle MeshStorage::get_vertex_data_buffer() const {
    return vertex_data_buffer;
}

BufferHandle MeshStorage::get_index_buffer() const {
    return index_buffer;
}
