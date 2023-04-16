#include "material_storage.hpp"
#include "render/backend/render_backend.hpp"

MaterialStorage::MaterialStorage(RenderBackend& backend_in) : backend{ backend_in }, material_upload{ backend } {
    auto& allocator = backend.get_global_allocator();
    material_buffer_handle = allocator.create_buffer(
        "Materials buffer", sizeof(BasicPbrMaterialGpu) * 65536,
        BufferUsage::StorageBuffer
    );
}

PooledObject<BasicPbrMaterialProxy> MaterialStorage::add_material(BasicPbrMaterial&& new_material) {
    auto& texture_descriptor_pool = backend.get_texture_descriptor_pool();

    new_material.gpu_data.base_color_texture_index = texture_descriptor_pool.create_texture_srv(
        new_material.base_color_texture, new_material.base_color_sampler
    );
    new_material.gpu_data.normal_texture_index = texture_descriptor_pool.create_texture_srv(
        new_material.normal_texture, new_material.normal_sampler
    );
    new_material.gpu_data.data_texture_index = texture_descriptor_pool.create_texture_srv(
        new_material.metallic_roughness_texture, new_material.metallic_roughness_sampler
    );
    new_material.gpu_data.emission_texture_index = texture_descriptor_pool.create_texture_srv(
        new_material.emission_texture, new_material.emission_sampler
    );

    const auto handle = material_pool.add_object(std::make_pair(new_material, MaterialProxy{}));
    auto& proxy = handle->second;
    
    material_upload.add_data(handle.index, new_material.gpu_data);

    // Pipelines......

    // I have some dream that objects can specify their own mesh functions and material functions, and that we can
    // dynamically build those into the vertex and fragment shaders that these pipelines need. Will take a while to
    // realize that dream

    // Animations wil be handled with a compute shader that runs before the other passes and which writes skinned
    // meshes to a new vertex buffer. Skinned objects will therefore need their own vertex buffers

    const auto cull_mode = static_cast<VkCullModeFlags>(new_material.double_sided
                                                            ? VK_CULL_MODE_NONE
                                                            : VK_CULL_MODE_BACK_BIT);
    const auto front_face = static_cast<VkFrontFace>(new_material.front_face_ccw
                                                         ? VK_FRONT_FACE_COUNTER_CLOCKWISE
                                                         : VK_FRONT_FACE_CLOCKWISE);

    // gbuffer
    const auto gbuffer_pipeline = backend.begin_building_pipeline(new_material.name)
                                         .set_vertex_shader("shaders/deferred/basic.vert.spv")
                                         .set_fragment_shader("shaders/deferred/standard_pbr.frag.spv")
                                         .set_blend_state(0, new_material.blend_state)
                                         .set_blend_state(1, new_material.blend_state)
                                         .set_blend_state(2, new_material.blend_state)
                                         .set_blend_state(3, new_material.blend_state)
                                         .set_raster_state(
                                             {
                                                 .cull_mode = cull_mode,
                                                 .front_face = front_face
                                             }
                                         )
                                         .build();
    proxy.pipelines.emplace(ScenePassType::Gbuffer, gbuffer_pipeline);

    // RSM
    const auto rsm_pipeline = backend.begin_building_pipeline(fmt::format("{} RSM", new_material.name))
                                     .set_vertex_shader("shaders/lpv/rsm.vert.spv")
                                     .set_fragment_shader("shaders/lpv/rsm.frag.spv")
                                     .set_blend_state(0, new_material.blend_state)
                                     .set_blend_state(1, new_material.blend_state)
                                     .set_raster_state(
                                         {
                                             .cull_mode = cull_mode,
                                             .front_face = front_face
                                         }
                                     )
                                     .build();
    proxy.pipelines.emplace(ScenePassType::RSM, rsm_pipeline);

    // Shadow
    const auto shadow_pipeline = backend.begin_building_pipeline(fmt::format("{} SHADOW", new_material.name))
                                        .set_vertex_shader("shaders/lighting/shadow.vert.spv")
                                        .set_raster_state(
                                            {
                                                .cull_mode = cull_mode,
                                                .front_face = front_face,
                                                .depth_clamp_enable = true
                                            }
                                        )
                                        .build();
    proxy.pipelines.emplace(ScenePassType::Shadow, shadow_pipeline);

    // Voxelization
    auto voxel_blend_state = new_material.blend_state;
    voxel_blend_state.colorWriteMask = 0;
    const auto voxelization_pipeline = backend.begin_building_pipeline(
                                                  fmt::format("{} VOXELIZATION", new_material.name)
                                              )
                                              .set_vertex_shader("shaders/voxelizer/voxelizer.vert.spv")
                                              .set_geometry_shader("shaders/voxelizer/voxelizer.geom.spv")
                                              .set_fragment_shader("shaders/voxelizer/voxelizer.frag.spv")
                                              .set_depth_state(
                                                  {
                                                      .enable_depth_test = false,
                                                      .enable_depth_write = false
                                                  }
                                              )
                                              .set_raster_state({.cull_mode = VK_CULL_MODE_NONE})
                                              .set_blend_state(0, voxel_blend_state)
                                              .build();
    proxy.pipelines.emplace(ScenePassType::Vozelization, voxelization_pipeline);

    return handle;
}

void MaterialStorage::destroy_material(PooledObject<BasicPbrMaterialProxy>&& proxy) {
    material_pool.free_object(proxy);
}

void MaterialStorage::flush_material_buffer(RenderGraph& graph) {
    material_upload.flush_to_buffer(graph, material_buffer_handle);
}

BufferHandle MaterialStorage::get_material_buffer() const { return material_buffer_handle; }
