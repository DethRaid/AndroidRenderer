#include "mesh_storage.hpp"

#include <tracy/Tracy.hpp>

#include "render/backend/resource_allocator.hpp"
#include "render/backend/resource_upload_queue.hpp"
#include "shared/vertex_data.hpp"

constexpr const uint32_t max_num_vertices = 1000000;
constexpr const uint32_t max_num_indices = 1000000;

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

    /*
     * Do a bunch of bullshit
     *
     * Some bullshits:
     * - Compute the area of each triangle in the mesh
     * - Average the area to get some concept of "average triangle area." This is how we should do LODs, any other
     *   method is cringe
     * - Sample the triangles using a weighted average of triangle area. Generate random positions with barycentrics.
     *   Use this to generate a representative point cloud of the mesh
     * - We can use this point cloud to build the GV for our LPVs
     * - We can use this point cloud for mesh lights. If the mesh has an emissive material, we can sample the emission
     *   texture at each point. Generate VPLs for each sample with non-zero emission and put them into a new buffer.
     *   Inject that buffer into the LPV before propagation
     * - This will make us win deccerballs
     */

    const auto [point_cloud, average_triangle_area] = generate_surface_point_cloud(vertices, indices);

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

/**
 * \brief Finds the reservoir that contains the probability sample
 *
 * Basically does a binary search among reservoir to find the one that's closest to the probability sample without being smaller
 *
 * \param probability_sample Probability to find a reservoir for
 * \param prefices Prefix sums of all the reservoir probabilities
 * \return Index of the reservoir that contains the probability
 */
size_t find_reservoir(double probability_sample, const std::vector<double>& prefices);

std::pair<std::vector<MeshPoint>, float> MeshStorage::generate_surface_point_cloud(
    std::span<const StandardVertex> vertices, std::span<const uint32_t> indices
) {
    ZoneScoped;

    auto triangle_areas = std::vector<float>{};
    triangle_areas.reserve(indices.size() / 3);

    auto area_accumulator = 0.f;

    for(uint32_t i = 0; i < indices.size(); i += 3) {
        const auto index_0 = indices[i];
        const auto index_1 = indices[i + 1];
        const auto index_2 = indices[i + 2];

        const auto& v0 = vertices[index_0];
        const auto& v1 = vertices[index_1];
        const auto& v2 = vertices[index_2];

        const auto edge_0 = v0.position - v1.position;
        const auto edge_1 = v0.position - v2.position;
        
        const auto parallelogram_area = glm::cross(edge_0, edge_1);

        const auto area = glm::length(parallelogram_area) / 2.0f;

        triangle_areas.push_back(area);
        area_accumulator += area;
    }

    // Normalize the area
    for (auto& area : triangle_areas) {
        area /= area_accumulator;
    }

    // Prefix sum my beloved
    auto prefices = std::vector<double>{};
    prefices.reserve(triangle_areas.size());

    auto last_prefix = 0.0;
    for(const auto area : triangle_areas) {
        last_prefix += area;
        prefices.emplace_back(last_prefix);
    }

    // area_accumulator is the total surface area of the mesh
    // We want a number of samples with a fixed density
    // We want one sample per 0.1 m^2
    // So the number of samples is the total area divided by 0.1
    const auto num_samples = glm::round(area_accumulator / 0.1f);
    
    auto points = std::vector<MeshPoint>{};
    points.reserve(static_cast<size_t>(num_samples));

    // TODO: Better rng
    srand(time(NULL));

    for(auto i = 0u; i < num_samples; i++) {
        const auto probability_sample = static_cast<double>(rand()) / static_cast<double>(RAND_MAX);
        const auto triangle_index = find_reservoir(probability_sample, prefices);
    }

    area_accumulator /= static_cast<float>(triangle_areas.size());

    return std::make_pair(std::vector<MeshPoint>{}, area_accumulator);
}

size_t find_reservoir(double probability_sample, const std::vector<double>& prefices) {
    // Find the index where prefices[n] > sample but prefices[n - 1] < sample

    // BINARY SEARCH
    auto test_index = prefices.size() / 2;

    if(prefices[test_index] > probability_sample) {
        if (test_index > 0 && prefices[test_index - 1] < probability_sample) {
            // WE FOUND IT
            return test_index;
        }


    }

    return 0;
}
