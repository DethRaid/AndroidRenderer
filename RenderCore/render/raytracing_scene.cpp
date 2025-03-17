#include "raytracing_scene.hpp"

#include "render/render_scene.hpp"
#include "render/backend/render_backend.hpp"
#include "console/cvars.hpp"
#include "render/mesh_storage.hpp"

[[maybe_unused]] static auto cvar_enable_raytracing = AutoCVar_Int{
    "r.Raytracing.Enable", "Whether or not to enable raytracing", 1
};

RaytracingScene::RaytracingScene(RenderScene& scene_in)
    : scene{scene_in} {}

void RaytracingScene::add_primitive(const MeshPrimitiveHandle primitive) {
    const auto& model_matrix = primitive->data.model;
    const auto blas_flags = primitive->material->first.transparency_mode == TransparencyMode::Solid ? VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR : VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR;

    // Multiply by two because each shader group has a GI and occlusion variant
    const auto sbt_offset = static_cast<uint32_t>(primitive->material->first.transparency_mode) * RenderBackend::get().get_shader_record_size() * 2;
    placed_blases.emplace_back(
        VkAccelerationStructureInstanceKHR{
            .transform = {
                .matrix = {
                    {model_matrix[0][0], model_matrix[0][1], model_matrix[0][2], model_matrix[0][3]},
                    {model_matrix[1][0], model_matrix[1][1], model_matrix[1][2], model_matrix[1][3]},
                    {model_matrix[2][0], model_matrix[2][1], model_matrix[2][2], model_matrix[2][3]}
                }
            },
            .instanceCustomIndex = primitive.index,
            .mask = 0xFF,
            .instanceShaderBindingTableRecordOffset = sbt_offset,
            .flags = static_cast<VkGeometryInstanceFlagsKHR>(blas_flags),
            .accelerationStructureReference = primitive->mesh->blas->as_address
        });

    is_dirty = true;
}

void RaytracingScene::finalize(RenderGraph& graph) {
    commit_tlas_builds(graph);
}

AccelerationStructureHandle RaytracingScene::get_acceleration_structure() const {
    return acceleration_structure;
}

void RaytracingScene::commit_tlas_builds(RenderGraph& graph) {
    if(!is_dirty) {
        return;
    }

    ZoneScoped;

    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();

    const auto instances_buffer = allocator.create_buffer(
        "RT instances buffer",
        sizeof(VkAccelerationStructureInstanceKHR) * placed_blases.size(),
        BufferUsage::StagingBuffer);

    auto* write_ptr = allocator.map_buffer(instances_buffer);
    std::memcpy(write_ptr, placed_blases.data(), instances_buffer->create_info.size);

    graph.add_transition_pass(
        {
            .buffers = {
                {
                    .buffer = instances_buffer,
                    .stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .access = VK_ACCESS_2_MEMORY_WRITE_BIT
                }
            }
        });

    // Put the above into a VkAccelerationStructureGeometryKHR. We need to put the instances struct in a union and label it as instance data.
    const auto tlas_geometry = VkAccelerationStructureGeometryKHR{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {
            .instances = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                .data = {.deviceAddress = instances_buffer->address}
            }
        }
    };

    // Find sizes
    auto build_info = VkAccelerationStructureBuildGeometryInfoKHR{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &tlas_geometry
    };

    const auto count_instance = static_cast<uint32_t>(placed_blases.size());
    auto size_info = VkAccelerationStructureBuildSizesInfoKHR{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    vkGetAccelerationStructureBuildSizesKHR(
        backend.get_device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &build_info,
        &count_instance,
        &size_info);

    acceleration_structure = allocator.create_acceleration_structure(
        size_info.accelerationStructureSize,
        VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR);

    const auto scratch_buffer = allocator.create_buffer(
        "TLAS build scratch buffer",
        size_info.buildScratchSize,
        BufferUsage::AccelerationStructure);
    allocator.destroy_buffer(scratch_buffer);

    // Update build information
    build_info.srcAccelerationStructure = VK_NULL_HANDLE;
    build_info.dstAccelerationStructure = acceleration_structure->acceleration_structure;
    build_info.scratchData.deviceAddress = scratch_buffer->address;

    graph.add_pass(
        {
            .name = "Build TLAS",
            .buffers = {
                {
                    .buffer = instances_buffer,
                    .stage = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                    .access = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR
                },
                {
                    .buffer = scratch_buffer,
                    .stage = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                    .access = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR
                },
                {
                    .buffer = acceleration_structure->buffer,
                    .stage = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                    .access = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR
                }
            },
            .execute = [=](const CommandBuffer& commands) {
                // Build Offsets info: n instances
                VkAccelerationStructureBuildRangeInfoKHR build_offset_info{count_instance, 0, 0, 0};
                const VkAccelerationStructureBuildRangeInfoKHR* p_build_offset_info = &build_offset_info;

                // Build the TLAS
                vkCmdBuildAccelerationStructuresKHR(commands.get_vk_commands(), 1, &build_info, &p_build_offset_info);
            }
        });

    is_dirty = false;
}
