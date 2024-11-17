#include "mesh_voxelizer.hpp"

#include "glm/ext/matrix_clip_space.hpp"
#include "render/mesh_storage.hpp"
#include "render/backend/pipeline_cache.hpp"
#include "render/backend/render_backend.hpp"
#include "shared/view_data.hpp"
#include "shared/voxelizer_compute_pass_parameters.hpp"

enum class VoxelizationMethod {
    RasterPipeline,
    ComputeShaders,
};

static AutoCVar_Enum<VoxelizationMethod> cvar_voxelization_method{
    "r.voxels.VoxelizationMethod",
    "How to voxelize meshes - raster pipeline or compute pipeline",
    VoxelizationMethod::ComputeShaders
};

MeshVoxelizer::MeshVoxelizer(RenderBackend& backend_in) : backend{&backend_in} {
    voxelization_pipeline = backend->begin_building_pipeline("Voxelizer")
                                   .set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                                   .set_vertex_shader("shaders/voxelizer/voxelizer.vert.spv")
                                   .set_geometry_shader("shaders/voxelizer/voxelizer.geom.spv")
                                   .set_fragment_shader("shaders/voxelizer/voxelizer.frag.spv")
                                   .set_depth_state(
                                       {
                                           .enable_depth_test = false, .enable_depth_write = false,
                                           .compare_op = VK_COMPARE_OP_ALWAYS
                                       }
                                   )
                                   .set_raster_state({.cull_mode = VK_CULL_MODE_NONE})
                                   .build();

    compute_voxelization_pipeline = backend->get_pipeline_cache()
                                           .create_pipeline("shaders/voxelizer/voxelizer.comp.spv");
}

VoxelTextures MeshVoxelizer::voxelize_with_raster(
    RenderGraph& graph, const MeshPrimitiveHandle primitive, const MeshStorage& mesh_storage,
    const BufferHandle primitive_buffer, const float voxel_size
) const {
    // Create a 3D texture big enough to hold the mesh's bounding sphere. There will be some wasted space, maybe we can copy to a smaller texture at some point?
    const auto bounds = primitive->mesh->bounds;
    const auto voxel_texture_resolution = glm::uvec3{(bounds.max - bounds.min) / voxel_size} + 1u;

    auto& allocator = backend->get_global_allocator();

    const auto voxels = allocator.create_volume_texture(
        "Mesh voxel buffer",
        VK_FORMAT_R8G8B8A8_UNORM,
        voxel_texture_resolution,
        1,
        TextureUsage::StorageImage
    );

    const auto frustums_buffer = allocator.create_buffer(
        "Voxelizer frustums",
        sizeof(glm::mat4),
        BufferUsage::StagingBuffer
    );
    auto* bounds_frustum_matrix = allocator.map_buffer<glm::mat4>(frustums_buffer);
    *bounds_frustum_matrix = glm::ortho(
        bounds.min.x,
        bounds.max.x,
        bounds.max.y,
        bounds.min.y,
        bounds.max.z,
        bounds.min.z
    );

    auto set = DescriptorSet{
                   *backend,
                backend->get_transient_descriptor_allocator(),
                   voxelization_pipeline->get_descriptor_set_info(0)
               }
               .bind(0, voxels)
               .bind(1, primitive_buffer)
               .bind(2, frustums_buffer)
               .finalize();
    graph.add_render_pass(
        DynamicRenderingPass{
            .name = "Voxelization",
            .descriptor_sets = {set},
            .execute = [this, &mesh_storage, &primitive, set = std::move(set)](CommandBuffer& commands) {
                commands.bind_vertex_buffer(0, mesh_storage.get_vertex_position_buffer());
                commands.bind_vertex_buffer(1, mesh_storage.get_vertex_data_buffer());
                commands.bind_index_buffer(mesh_storage.get_index_buffer());

                commands.bind_descriptor_set(0, set.get_vk_descriptor_set());
                commands.bind_descriptor_set(1, backend->get_texture_descriptor_pool().get_descriptor_set());

                commands.set_push_constant(0, primitive.index);

                commands.bind_pipeline(voxelization_pipeline);

                const auto& mesh = primitive->mesh;
                commands.draw_indexed(
                    mesh->num_indices,
                    1,
                    static_cast<uint32_t>(mesh->first_index),
                    static_cast<uint32_t>(mesh->first_vertex),
                    0
                );
            }
        });

    return {.num_voxels = voxel_texture_resolution, .color_texture = voxels};
}

VoxelTextures MeshVoxelizer::voxelize_with_compute(
    RenderGraph& graph, const MeshPrimitiveHandle primitive, const MeshStorage& mesh_storage,
    const BufferHandle primitive_buffer,
    const float voxel_size
) const {
    // Implementation of https://bronsonzgeb.com/index.php/2021/05/22/gpu-mesh-voxelizer-part-1/
    // Naive compute-based voxelizer that tests every triangle against every voxel. Not ideal but potentially good enough

    // Create a 3D texture big enough to hold the mesh's bounding box. There will be some wasted space, maybe we can copy to a smaller texture at some point?
    const auto bounds = primitive->mesh->bounds;
    const auto voxel_texture_resolution = glm::uvec3{(bounds.max - bounds.min) / voxel_size} + 1u;

    auto& allocator = backend->get_global_allocator();

    const auto voxels_color = allocator.create_volume_texture(
        "Mesh voxel colors",
        VK_FORMAT_R8G8B8A8_UNORM,
        voxel_texture_resolution,
        1,
        TextureUsage::StorageImage
    );

    const auto voxels_normal = allocator.create_volume_texture(
        "Mesh voxel normals",
        VK_FORMAT_R16G16B16A16_SNORM,
        voxel_texture_resolution,
        1,
        TextureUsage::StorageImage
    );

    const auto pass_parameters = VoxelizerComputePassParameters{
        .bounds_min = glm::vec4{bounds.min, 0.f},
        .half_cell_size = voxel_size * 0.5f,
        .primitive_index = primitive.index,
    };

    auto descriptor_set = backend->get_transient_descriptor_allocator()
                                 .create_set(compute_voxelization_pipeline, 0)
                                 .bind(0, mesh_storage.get_vertex_position_buffer())
                                 .bind(1, mesh_storage.get_vertex_data_buffer())
                                 .bind(2, mesh_storage.get_index_buffer())
                                 .bind(3, primitive_buffer)
                                 .bind(4, mesh_storage.get_draw_args_buffer())
                                 .bind(5, voxels_color)
                                 .bind(6, voxels_normal)
                                 .finalize();

    graph.add_compute_dispatch<VoxelizerComputePassParameters>(
        {
            .name = "Voxelize",
            .descriptor_sets = {descriptor_set},
            .push_constants = pass_parameters,
            .num_workgroups = voxel_texture_resolution,
            .compute_shader = compute_voxelization_pipeline
        }
    );

    return {.num_voxels = voxel_texture_resolution, .color_texture = voxels_color, .normals_texture = voxels_normal};
}

VoxelTextures MeshVoxelizer::voxelize_primitive(
    RenderGraph& graph, const MeshPrimitiveHandle primitive, const MeshStorage& mesh_storage,
    const BufferHandle primitive_buffer, const float voxel_size, const Mode mode
) const {
    switch(cvar_voxelization_method.Get()) {
    case VoxelizationMethod::RasterPipeline:
        return voxelize_with_raster(graph, primitive, mesh_storage, primitive_buffer, voxel_size);

    case VoxelizationMethod::ComputeShaders:
        return voxelize_with_compute(graph, primitive, mesh_storage, primitive_buffer, voxel_size);
    }

    return {};
}
