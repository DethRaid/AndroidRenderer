#include "material_storage.hpp"
#include "render/backend/render_backend.hpp"

MaterialStorage::MaterialStorage(RenderBackend& backend_in) : backend{backend_in} {
    auto& allocator = backend.get_global_allocator();
    material_buffer_handle = allocator.create_buffer(
        "Materials buffer", sizeof(BasicPbrMaterialGpu) * 65536,
        BufferUsage::UniformBuffer
    );
}

PooledObject<BasicPbrMaterialProxy> MaterialStorage::add_material(BasicPbrMaterial&& new_material) {
    const auto handle = materials.add_object(std::make_pair(new_material, MaterialProxy{}));
    auto& proxy = handle->second;

    // Descriptor set...

    auto descriptor_builder = backend.create_persistent_descriptor_builder();
    const auto& resources = backend.get_global_allocator();

    const auto& base_color_texture = resources.get_texture(new_material.base_color_texture);
    auto base_color_info = VkDescriptorImageInfo{
        .sampler = new_material.base_color_sampler,
        .imageView = base_color_texture.image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    descriptor_builder.bind_image(
        0, &base_color_info, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT
    );

    const auto& normal_texture = resources.get_texture(new_material.normal_texture);
    auto normal_texture_info = VkDescriptorImageInfo{
        .sampler = new_material.normal_sampler,
        .imageView = normal_texture.image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    descriptor_builder.bind_image(
        1, &normal_texture_info, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_SHADER_STAGE_FRAGMENT_BIT
    );

    const auto& metallic_roughness_texture = resources.get_texture(new_material.metallic_roughness_texture);
    auto metallic_roughness_texture_info = VkDescriptorImageInfo{
        .sampler = new_material.metallic_roughness_sampler,
        .imageView = metallic_roughness_texture.image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    descriptor_builder.bind_image(
        2, &metallic_roughness_texture_info, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_SHADER_STAGE_FRAGMENT_BIT
    );

    const auto& emission_texture = resources.get_texture(new_material.emission_texture);
    auto emission_texture_info = VkDescriptorImageInfo{
        .sampler = new_material.emission_sampler,
        .imageView = emission_texture.image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    descriptor_builder.bind_image(
        3, &emission_texture_info, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_SHADER_STAGE_FRAGMENT_BIT
    );

    auto& material_buffer = resources.get_buffer(material_buffer_handle);
    auto* materials_pointer = static_cast<BasicPbrMaterialGpu*>(material_buffer.allocation_info.pMappedData);
    auto& material_pointer = materials_pointer[handle.index];
    std::memcpy(&material_pointer, &new_material.gpu_data, sizeof(BasicPbrMaterialGpu));

    auto uniform_info = VkDescriptorBufferInfo{
        .buffer = material_buffer.buffer,
        .offset = handle.index * sizeof(BasicPbrMaterialGpu),
        .range = sizeof(BasicPbrMaterialGpu)
    };
    descriptor_builder.bind_buffer(4, &uniform_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);

    proxy.descriptor_set = *descriptor_builder.build();

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

void MaterialStorage::flush_material_buffer(CommandBuffer& commands) {
    commands.flush_buffer(material_buffer_handle);
}
