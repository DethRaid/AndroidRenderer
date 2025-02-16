#include "blas_build_queue.hpp"

#include <tracy/Tracy.hpp>

#include "command_buffer.hpp"
#include "render_backend.hpp"
#include "console/cvars.hpp"

static auto cvar_max_concurrent_builds = AutoCVar_Int{
    "r.RHI.BlasBuildBatchSize",
    "Size of each batch of BLAS builds. Larger builds allow more overlap on the GPU, but use more memory", 8
};

BlasBuildQueue::BlasBuildQueue() {
    pending_jobs.reserve(128);
}

void BlasBuildQueue::enqueue(AccelerationStructureHandle blas, const VkAccelerationStructureGeometryKHR& create_info) {
    pending_jobs.emplace_back(blas, create_info);
}

void BlasBuildQueue::flush_pending_builds(RenderGraph& graph) {
    ZoneScoped;

    if(pending_jobs.empty()) {
        return;
    }

    graph.begin_label("BLAS builds");

    uint64_t max_scratch_buffer_size = 0;
    for(const auto& job : pending_jobs) {
        max_scratch_buffer_size = std::max(max_scratch_buffer_size, job.handle->scratch_buffer_size);
    }

    const auto batch_size = cvar_max_concurrent_builds.Get();

    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();
    const auto scratch_buffer = allocator.create_buffer(
        "Scratch buffer",
        max_scratch_buffer_size * batch_size,
        BufferUsage::StorageBuffer);

    allocator.destroy_buffer(scratch_buffer);

    for(auto i = 0u; i < pending_jobs.size(); i += batch_size) {
        auto barriers = std::vector<BufferUsageToken>{
            {
                .buffer = scratch_buffer,
                .stage = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                .access = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR
            }
        };

        auto build_geometry_infos = std::vector<VkAccelerationStructureBuildGeometryInfoKHR>{};
        auto build_range_infos = std::vector<VkAccelerationStructureBuildRangeInfoKHR>{};
        auto build_range_info_ptrs = std::vector<VkAccelerationStructureBuildRangeInfoKHR*>{};
        build_geometry_infos.reserve(batch_size);
        build_range_infos.reserve(batch_size);
        build_range_info_ptrs.reserve(batch_size);

        auto scratch_buffer_address = scratch_buffer->address;

        for(auto job_idx = i; job_idx < i + batch_size; job_idx++) {
            if(job_idx >= pending_jobs.size()) {
                break;
            }

            const auto& job = pending_jobs[job_idx];

            barriers.emplace_back(
                BufferUsageToken{
                    .buffer = pending_jobs[job_idx].handle->buffer,
                    .stage = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                    .access = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR
                });

            build_geometry_infos.emplace_back(
                VkAccelerationStructureBuildGeometryInfoKHR{
                    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                    .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
                    .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
                    .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
                    .dstAccelerationStructure = job.handle->acceleration_structure,
                    .geometryCount = 1,
                    .pGeometries = &job.create_info,
                    .scratchData = {.deviceAddress = scratch_buffer_address},
                });

            scratch_buffer_address += job.handle->scratch_buffer_size;

            build_range_infos.emplace_back(
                VkAccelerationStructureBuildRangeInfoKHR{
                    .primitiveCount = job.handle->num_triangles,
                    .primitiveOffset = 0,
                    .firstVertex = 0,
                    .transformOffset = 0,
                });
            build_range_info_ptrs.emplace_back(&build_range_infos.back());
        }

        graph.add_pass(
            {
                .name = "BLAS builds",
                .buffers = barriers,
                .execute = [
                    &backend,
                    build_geometry_infos=std::move(build_geometry_infos),
                    build_range_infos=std::move(build_range_infos),
                    build_range_info_ptrs=std::move(build_range_info_ptrs)]
            (const CommandBuffer& commands) {
                    TracyVkZone(backend.get_tracy_context(), commands.get_vk_commands(), "BLAS Build");

                    commands.build_acceleration_structures(build_geometry_infos, build_range_info_ptrs);
                }
            });
    }

    graph.end_label();

    pending_jobs.clear();
}
