#include "blas_build_queue.hpp"

#include <tracy/Tracy.hpp>

#include "command_buffer.hpp"
#include "render_backend.hpp"

BlasBuildQueue::BlasBuildQueue(RenderBackend& backend_in) : backend{backend_in} {
    pending_jobs.reserve(128);
}

void BlasBuildQueue::enqueue(AccelerationStructureHandle blas, const VkAccelerationStructureGeometryKHR& create_info) {
    pending_jobs.emplace_back(blas, create_info);
}

void BlasBuildQueue::flush_pending_builds(CommandBuffer& commands) {
    ZoneScoped;

    commands.begin_label(__func__);
    TracyVkZone(backend.get_tracy_context(), commands.get_vk_commands(), __func__);

    auto& allocator = backend.get_global_allocator();

    auto build_geometry_infos = std::vector<VkAccelerationStructureBuildGeometryInfoKHR>{};
    auto build_range_infos = std::vector<VkAccelerationStructureBuildRangeInfoKHR>{};
    auto build_range_info_ptrs = std::vector<VkAccelerationStructureBuildRangeInfoKHR*>{};
    build_geometry_infos.reserve(pending_jobs.size());
    build_range_infos.reserve(pending_jobs.size());
    build_range_info_ptrs.reserve(pending_jobs.size());

    for(const auto& job : pending_jobs) {
        const auto scratch_buffer = allocator.create_buffer(
            "Scratch buffer",
            job.handle->scratch_buffer_size,
            BufferUsage::StorageBuffer);

        build_geometry_infos.emplace_back(
            VkAccelerationStructureBuildGeometryInfoKHR{
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
                .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
                .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
                .dstAccelerationStructure = job.handle->acceleration_structure,
                .geometryCount = 1,
                .pGeometries = &job.create_info,
                .scratchData = {.deviceAddress = scratch_buffer->address},
            });

        build_range_infos.emplace_back(VkAccelerationStructureBuildRangeInfoKHR{
            .primitiveCount = job.handle->num_triangles,
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0,
        });
        build_range_info_ptrs.emplace_back(&build_range_infos.back());
    }

    commands.build_acceleration_structures(build_geometry_infos, build_range_info_ptrs);

    pending_jobs.clear();

    commands.end_label();
}
