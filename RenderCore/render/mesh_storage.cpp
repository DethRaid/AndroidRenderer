#include "mesh_storage.hpp"

#include <random>
#include <tracy/Tracy.hpp>

#include "backend/blas_build_queue.hpp"
#include "core/system_interface.hpp"
#include "render/backend/resource_allocator.hpp"
#include "render/backend/resource_upload_queue.hpp"
#include "shared/vertex_data.hpp"

constexpr uint32_t max_num_meshes = 65536;
constexpr const uint32_t max_num_vertices = 100000000;
constexpr const uint32_t max_num_indices = 100000000;

static std::shared_ptr<spdlog::logger> logger;

MeshStorage::MeshStorage() {
    if(logger == nullptr) {
        logger = SystemInterface::get().get_logger("MeshStorage");
        logger->set_level(spdlog::level::info);
    }

    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();
    vertex_position_buffer = allocator.create_buffer(
        "Vertex position buffer",
        max_num_vertices * sizeof(VertexPosition),
        BufferUsage::VertexBuffer
    );
    vertex_data_buffer = allocator.create_buffer(
        "Vertex data buffer",
        max_num_vertices * sizeof(StandardVertexData),
        BufferUsage::VertexBuffer
    );
    index_buffer = allocator.create_buffer(
        "Index buffer",
        max_num_indices * sizeof(uint32_t),
        BufferUsage::IndexBuffer
    );

    mesh_draw_args_buffer = allocator.create_buffer(
        "Mesh draw args buffer",
        sizeof(VkDrawIndexedIndirectCommand) * max_num_meshes,
        BufferUsage::StorageBuffer);

    constexpr auto vertex_block_create_info = VmaVirtualBlockCreateInfo{
        .size = max_num_vertices,
    };
    vmaCreateVirtualBlock(&vertex_block_create_info, &vertex_block);

    constexpr auto index_block_create_info = VmaVirtualBlockCreateInfo{
        .size = max_num_indices,
    };
    vmaCreateVirtualBlock(&index_block_create_info, &index_block);
}

MeshStorage::~MeshStorage() {
    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();
    allocator.destroy_buffer(vertex_position_buffer);
    allocator.destroy_buffer(vertex_data_buffer);
    allocator.destroy_buffer(index_buffer);
    allocator.destroy_buffer(mesh_draw_args_buffer);

    // Yeet all the meshes, even if not explicitly destroyed
    vmaClearVirtualBlock(vertex_block);
    vmaClearVirtualBlock(index_block);
    vmaDestroyVirtualBlock(vertex_block);
    vmaDestroyVirtualBlock(index_block);
}

tl::optional<MeshHandle> MeshStorage::add_mesh(
    const std::span<const StandardVertex> vertices, const std::span<const uint32_t> indices, const Box& bounds
) {
    auto mesh = Mesh{};

    const auto vertex_allocate_info = VmaVirtualAllocationCreateInfo{
        .size = vertices.size(),
    };
    auto result = vmaVirtualAllocate(vertex_block, &vertex_allocate_info, &mesh.vertex_allocation, &mesh.first_vertex);
    if(result != VK_SUCCESS) {
        return tl::nullopt;
    }

    const auto index_allocate_info = VmaVirtualAllocationCreateInfo{
        .size = indices.size(),
    };
    result = vmaVirtualAllocate(index_block, &index_allocate_info, &mesh.index_allocation, &mesh.first_index);
    if(result != VK_SUCCESS) {
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

    for(const auto& vertex : vertices) {
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

    auto& backend = RenderBackend::get();
    auto& upload_queue = backend.get_upload_queue();
    upload_queue.upload_to_buffer<VertexPosition>(
        vertex_position_buffer,
        positions,
        static_cast<uint32_t>(mesh.first_vertex * sizeof(VertexPosition))
    );
    upload_queue.upload_to_buffer<StandardVertexData>(
        vertex_data_buffer,
        data,
        static_cast<uint32_t>(mesh.first_vertex *
            sizeof(StandardVertexData))
    );
    upload_queue.upload_to_buffer(index_buffer, indices, static_cast<uint32_t>(mesh.first_index * sizeof(uint32_t)));

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

    mesh.average_triangle_area = average_triangle_area;

    auto& allocator = backend.get_global_allocator();
    mesh.point_cloud_buffer = allocator.create_buffer(
        fmt::format("Mesh point cloud"),
        sizeof(StandardVertex) * point_cloud.size(),
        BufferUsage::StorageBuffer
    );
    upload_queue.upload_to_buffer(mesh.point_cloud_buffer, std::span{point_cloud}, 0);

    mesh.sh_points_buffer = generate_sh_point_cloud(point_cloud);
    mesh.num_points = static_cast<uint32_t>(point_cloud.size());

    const auto handle = meshes.add_object(std::move(mesh));

    if(mesh_draw_args_upload_buffer.is_full()) {
        auto graph = backend.create_render_graph();
        flush_mesh_draw_arg_uploads(graph);
        graph.finish();
        backend.execute_graph(std::move(graph));
    }

    mesh_draw_args_upload_buffer.add_data(
        handle.index,
        {
            .indexCount = handle->num_indices,
            .instanceCount = 1,
            .firstIndex = static_cast<uint32_t>(handle->first_index),
            .vertexOffset = static_cast<int32_t>(handle->first_vertex),
            .firstInstance = 0
        }
    );

    if(backend.use_ray_tracing()) {
        handle->blas = create_blas_for_mesh(
            static_cast<uint32_t>(handle->first_vertex),
            handle->num_vertices,
            static_cast<uint32_t>(handle->first_index),
            handle->num_indices / 3
        );
    }

    return handle;
}

void MeshStorage::free_mesh(const MeshHandle mesh) {
    vmaVirtualFree(vertex_block, mesh->vertex_allocation);
    vmaVirtualFree(index_block, mesh->index_allocation);

    meshes.free_object(mesh);
}

void MeshStorage::flush_mesh_draw_arg_uploads(RenderGraph& graph) {
    if(mesh_draw_args_upload_buffer.get_size() > 0) {
        mesh_draw_args_upload_buffer.flush_to_buffer(graph, mesh_draw_args_buffer);
    }
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

BufferHandle MeshStorage::get_draw_args_buffer() const {
    return mesh_draw_args_buffer;
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

std::pair<std::vector<StandardVertex>, float> MeshStorage::generate_surface_point_cloud(
    const std::span<const StandardVertex> vertices, const std::span<const uint32_t> indices
) const {
    ZoneScoped;

    auto triangle_areas = std::vector<double>{};
    triangle_areas.reserve(indices.size() / 3);

    auto area_accumulator = 0.0;

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

        const auto area = glm::length(parallelogram_area) / 2.0;

        triangle_areas.push_back(area);
        area_accumulator += area;
    }

    // Normalize the area
    for(auto& area : triangle_areas) {
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
    auto num_samples = static_cast<size_t>(glm::ceil(area_accumulator / 0.1));
    num_samples = glm::min(num_samples, 65536ull);

    auto points = std::vector<StandardVertex>{};
    points.reserve(num_samples);

    auto r = std::random_device{};
    auto e1 = std::default_random_engine{r()};
    auto uniform_dist = std::uniform_real_distribution<double>{0.0, 1.0};

    for(auto i = 0u; i < num_samples; i++) {
        const auto probability_sample = uniform_dist(e1);
        logger->trace("Searching for reservoir {}", probability_sample);
        const auto triangle_id = find_reservoir(probability_sample, prefices);
        const auto barycentric = glm::normalize(glm::vec3{uniform_dist(e1), uniform_dist(e1), uniform_dist(e1)});

        const auto vertex = interpolate_vertex(vertices, indices, triangle_id, barycentric);

        points.emplace_back(vertex);
    }

    area_accumulator /= static_cast<double>(triangle_areas.size());

    return std::make_pair(points, static_cast<float>(area_accumulator));
}

StandardVertex MeshStorage::interpolate_vertex(
    std::span<const StandardVertex> vertices, std::span<const uint32_t> indices, size_t triangle_id,
    glm::vec3 barycentric
) {
    const auto provoking_index = triangle_id * 3;
    const auto i0 = indices[provoking_index];
    const auto i1 = indices[provoking_index + 1];
    const auto i2 = indices[provoking_index + 2];

    const auto& v0 = vertices[i0];
    const auto& v1 = vertices[i1];
    const auto& v2 = vertices[i2];

    // Barycentrib
    const auto p0 = v0.position * barycentric.x;
    const auto p1 = v1.position * barycentric.y;
    const auto p2 = v2.position * barycentric.z;

    const auto position = (p0 + p1 + p2) / 3.f;

    const auto n0 = v0.normal * barycentric.x;
    const auto n1 = v1.normal * barycentric.y;
    const auto n2 = v2.normal * barycentric.z;

    const auto normal = (n0 + n1 + n2) / 3.f;

    const auto t0 = v0.tangent * barycentric.x;
    const auto t1 = v1.tangent * barycentric.y;
    const auto t2 = v2.tangent * barycentric.z;

    const auto tangent = (t0 + t1 + t2) / 3.f;

    const auto uv0 = v0.texcoord * barycentric.x;
    const auto uv1 = v1.texcoord * barycentric.y;
    const auto uv2 = v2.texcoord * barycentric.z;

    const auto texcoord = (uv0 + uv1 + uv2) / 3.f;

    const auto c0 = glm::unpackUnorm4x8(v0.color) * barycentric.x;
    const auto c1 = glm::unpackUnorm4x8(v1.color) * barycentric.y;
    const auto c2 = glm::unpackUnorm4x8(v2.color) * barycentric.z;

    const auto color = (c0 + c1 + c2) / 3.f;

    return {
        .position = position,
        .normal = normal,
        .tangent = tangent,
        .texcoord = texcoord,
        .color = glm::packUnorm4x8(color),
    };
}

// Vector to SH - from https://ericpolman.com/2016/06/28/light-propagation-volumes/

/*Spherical harmonics coefficients precomputed*/
// 1 / 2sqrt(pi)
#define SH_c0 0.282094792f
// sqrt(3/pi) / 2
#define SH_c1 0.488602512f

/*Cosine lobe coeff*/
// sqrt(pi)/2
#define SH_cosLobe_c0 0.886226925f
// sqrt(pi/3) 
#define SH_cosLobe_c1 1.02332671f

glm::vec4 dir_to_cosine_lobe(const glm::vec3 dir) {
    return glm::vec4{SH_cosLobe_c0, -SH_cosLobe_c1 * dir.y, SH_cosLobe_c1 * dir.z, -SH_cosLobe_c1 * dir.x};
}

glm::vec4 dir_to_sh(const glm::vec3 dir) {
    return glm::vec4{SH_c0, -SH_c1 * dir.y, SH_c1 * dir.z, -SH_c1 * dir.x};
}

BufferHandle MeshStorage::generate_sh_point_cloud(const std::vector<StandardVertex>& point_cloud) const {
    auto sh_points = std::vector<ShPoint>{};
    sh_points.reserve(point_cloud.size());

    for(const auto& point : point_cloud) {
        const auto sh = dir_to_cosine_lobe(point.normal);
        sh_points.emplace_back(glm::vec4{point.position, 1.f}, sh);
    }

    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();
    const auto sh_buffer_handle = allocator.create_buffer(
        "SH Point Cloud",
        sizeof(ShPoint) * sh_points.size(),
        BufferUsage::StorageBuffer
    );
    auto& upload_queue = backend.get_upload_queue();
    upload_queue.upload_to_buffer(sh_buffer_handle, std::span{sh_points}, 0);

    return sh_buffer_handle;
}

size_t find_reservoir(const double probability_sample, const std::vector<double>& prefices) {
    // Find the index where prefices[n] > sample but prefices[n - 1] < sample

    // BINARY SEARCH
    auto test_index = prefices.size() / 2;
    auto step_size = test_index;

    while(true) {
        if(prefices[test_index] > probability_sample) {
            if(test_index == 0) {
                // The lowest reservoir contains the sample
                return test_index;
            }
            if(test_index > 0 && prefices[test_index - 1] < probability_sample) {
                // We're not in the lowest reservoir, but we did find the sample
                return test_index;
            }
        }

        if(prefices[test_index] < probability_sample && test_index == prefices.size() - 1) {
            // The probability sample is outside of the range - just return the last index
            return test_index;
        }

        // We have not yet found the reservoir. Change the test index and try again
        step_size = glm::max(step_size / 2u, static_cast<size_t>(1u));
        if(prefices[test_index] > probability_sample) {
            test_index -= step_size;
        } else {
            test_index += step_size;
        }

        // Pray we avoid infinite loops
    }

    throw std::runtime_error{"Could not find appropriate reservoir"};
}

AccelerationStructureHandle MeshStorage::create_blas_for_mesh(
    const uint32_t first_vertex, const uint32_t num_vertices, const uint32_t first_index, const uint num_triangles
) const {
    ZoneScoped;

    auto& backend = RenderBackend::get();

    const auto geometry = VkAccelerationStructureGeometryKHR{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = {
            .triangles = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                .vertexData = {.deviceAddress = vertex_position_buffer->address + first_vertex * sizeof(glm::vec3)},
                .vertexStride = sizeof(glm::vec3),
                .maxVertex = num_vertices - 1,
                .indexType = VK_INDEX_TYPE_UINT32,
                .indexData = {.deviceAddress = index_buffer->address + first_index * sizeof(uint32_t)}
            }
        },
    };

    const auto build_info = VkAccelerationStructureBuildGeometryInfoKHR{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry,
    };
    auto size_info = VkAccelerationStructureBuildSizesInfoKHR{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    vkGetAccelerationStructureBuildSizesKHR(
        backend.get_device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &build_info,
        &num_triangles,
        &size_info);

    const auto as = backend.get_global_allocator()
                           .create_acceleration_structure(
                               static_cast<uint32_t>(size_info.accelerationStructureSize),
                               VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR);

    as->scratch_buffer_size = size_info.buildScratchSize;

    backend.get_blas_build_queue().enqueue(as, geometry);

    return as;
}
