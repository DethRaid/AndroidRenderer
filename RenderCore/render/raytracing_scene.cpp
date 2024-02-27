#include "raytracing_scene.hpp"

#include "render/render_scene.hpp"
#include "backend/render_backend.hpp"
#include "console/cvars.hpp"
#include "render/mesh_storage.hpp"

static auto cvar_enable_raytracing = AutoCVar_Int{
    "r.Raytracing.Enable", "Whether or not to enable raytracing", 0
};

RaytracingScene::RaytracingScene(RenderBackend& backend_in, RenderScene& scene_in)
    : backend{backend_in}, scene{scene_in} {
    const auto create_info = VkAccelerationStructureCreateInfoKHR{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .createFlags = VK_ACCELERATION_STRUCTURE_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_KHR,
    };
    vkCreateAccelerationStructureKHR(backend.get_device(), &create_info, nullptr, &opaque_scene);
}

void RaytracingScene::create_blas_for_mesh(const MeshHandle mesh) {
    auto& allocator = backend.get_global_allocator();

    const auto vertex_buffer = scene.get_meshes().get_vertex_position_buffer();
    const auto& vertex_buffer_actual = allocator.get_buffer(vertex_buffer);
    auto vertex_buffer_pointer = vertex_buffer_actual.address;

    auto vertex_device_address = VkDeviceAddress{0};
    vertex_device_address += vertex_buffer_pointer.x;
    vertex_device_address += static_cast<uint64_t>(vertex_buffer_pointer.y) << 32;
    vertex_device_address += mesh->first_vertex * sizeof(VertexPosition);

    const auto index_buffer = scene.get_meshes().get_index_buffer();
    const auto& index_buffer_actual = allocator.get_buffer(index_buffer);
    const auto index_buffer_pointer = index_buffer_actual.address;

    auto index_device_address = VkDeviceAddress{0};
    index_device_address += index_buffer_pointer.x;
    index_device_address += static_cast<uint64_t>(index_buffer_pointer.y) << 32;
    index_device_address += mesh->first_index * sizeof(uint32_t);

    const auto triangles = VkAccelerationStructureGeometryTrianglesDataKHR{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = {.deviceAddress = vertex_device_address},
        .vertexStride = sizeof(VertexPosition),
        .maxVertex = mesh->num_vertices,
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = {.deviceAddress = index_device_address},
    };

    const auto geometry = VkAccelerationStructureGeometryKHR{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = {.triangles = triangles},
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    };

    const auto range_build_info = VkAccelerationStructureBuildRangeInfoKHR{
        .primitiveCount = mesh->num_indices / 3,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0,
    };

    // TODO: Store these structs somewhere, put them in a hash map indexed by handle or something. Build them in a specific method at beginning of frame
    // TODO: Make one big geometry triangles data for the entire mesh buffer, make individual build infos for each mesh?

    pending_blas_geometries.push_back(geometry);
    pending_blas_ranges.push_back(range_build_info);
}

void RaytracingScene::add_primitive(MeshPrimitiveHandle primitive) { }

void RaytracingScene::finalize() {
    commit_blas_builds();

    commit_tlas_builds();
}

void RaytracingScene::commit_blas_builds() {
    auto& allocator = backend.get_global_allocator();

    auto total_as_size = VkDeviceSize{0};
    auto scratch_buffer_size = VkDeviceSize{0};

    for (auto mesh_index = 0u; mesh_index < pending_blas_geometries.size(); mesh_index++) {
        const auto build_geometry_info = VkAccelerationStructureBuildGeometryInfoKHR{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .dstAccelerationStructure = opaque_scene,
            .geometryCount = static_cast<uint32_t>(pending_blas_geometries.size()),
            .pGeometries = pending_blas_geometries.data(),
        };

        auto build_sizes_info = VkAccelerationStructureBuildSizesInfoKHR{};
        auto max_primitive_counts = std::vector<uint32_t>{};
        max_primitive_counts.reserve(pending_blas_ranges.size());
        for (const auto& blas_range : pending_blas_ranges) {
            max_primitive_counts.push_back(blas_range.primitiveCount);
        }
        vkGetAccelerationStructureBuildSizesKHR(
            backend.get_device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_geometry_info,
            max_primitive_counts.data(), &build_sizes_info
        );

        total_as_size += build_sizes_info.accelerationStructureSize;
        scratch_buffer_size = std::max(scratch_buffer_size, build_sizes_info.buildScratchSize);
    }

    const auto scratch_buffer = allocator.create_buffer(
        "BLAS build scratch buffer", scratch_buffer_size, BufferUsage::StorageBuffer
    );
    const auto& scratch_buffer_actual = allocator.get_buffer(scratch_buffer);

    auto graph = RenderGraph{backend};
    graph.add_pass(
        {
            .name = "Build BLASes",
            .buffers = {
                {
                    scratch_buffer,
                    {
                        VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                        VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR |
                        VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR
                    }
                }
            },
            .execute = [&](CommandBuffer& commands) {}
        }
    );
}

void RaytracingScene::commit_tlas_builds() {
    // TODO
}
