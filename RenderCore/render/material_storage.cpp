#include "material_storage.hpp"
#include "render/backend/render_backend.hpp"

MaterialStorage::MaterialStorage() {
    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();
    material_buffer_handle = allocator.create_buffer(
        "Materials buffer",
        sizeof(BasicPbrMaterialGpu) * 65536,
        BufferUsage::StorageBuffer
    );
}

PooledObject<BasicPbrMaterialProxy> MaterialStorage::add_material(BasicPbrMaterial&& new_material) {
    ZoneScoped;
    auto& backend = RenderBackend::get();
    auto& texture_descriptor_pool = backend.get_texture_descriptor_pool();

    new_material.gpu_data.base_color_texture_index = texture_descriptor_pool.create_texture_srv(
        new_material.base_color_texture,
        new_material.base_color_sampler
    );
    new_material.gpu_data.normal_texture_index = texture_descriptor_pool.create_texture_srv(
        new_material.normal_texture,
        new_material.normal_sampler
    );
    new_material.gpu_data.data_texture_index = texture_descriptor_pool.create_texture_srv(
        new_material.metallic_roughness_texture,
        new_material.metallic_roughness_sampler
    );
    new_material.gpu_data.emission_texture_index = texture_descriptor_pool.create_texture_srv(
        new_material.emission_texture,
        new_material.emission_sampler
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

    VkPipelineColorBlendAttachmentState blend_state;
    switch(new_material.transparency_mode) {
    case TransparencyMode::Solid:
        [[fallthrough]];
    case TransparencyMode::Cutout:
        blend_state = {
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT
        };
        break;

    case TransparencyMode::Translucent:
        blend_state = {
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };
        break;
    }

    // Depth prepass
    const auto depth_prepass_pipeline = backend.begin_building_pipeline(
                                                   fmt::format("{} depth prepass", new_material.name)
                                               )
                                               .set_vertex_shader("shaders/deferred/basic.vert.spv")
                                               .set_raster_state(
                                                   {
                                                       .cull_mode = cull_mode,
                                                       .front_face = front_face
                                                   }
                                               )
                                               .enable_dgc()
                                               .build();
    proxy.pipelines[ScenePassType::DepthPrepass] = depth_prepass_pipeline;

    // gbuffer
    const auto gbuffer_pipeline = backend.begin_building_pipeline(new_material.name)
                                         .set_vertex_shader("shaders/deferred/basic.vert.spv")
                                         .set_fragment_shader("shaders/deferred/standard_pbr.frag.spv")
                                         .set_blend_state(0, blend_state)
                                         .set_blend_state(1, blend_state)
                                         .set_blend_state(2, blend_state)
                                         .set_blend_state(3, blend_state)
                                         .set_depth_state(
                                             {
                                                 .enable_depth_test = true,
                                                 .enable_depth_write = false,
                                                 .compare_op = VK_COMPARE_OP_EQUAL
                                             }
                                         )
                                         .set_raster_state(
                                             {
                                                 .cull_mode = cull_mode,
                                                 .front_face = front_face
                                             }
                                         )
                                         .build();
    proxy.pipelines[ScenePassType::Gbuffer] = gbuffer_pipeline;

    // RSM
    const auto rsm_pipeline = backend.begin_building_pipeline(fmt::format("{} RSM", new_material.name))
                                     .set_vertex_shader("shaders/lpv/rsm.vert.spv")
                                     .set_fragment_shader("shaders/lpv/rsm.frag.spv")
                                     .set_blend_state(0, blend_state)
                                     .set_blend_state(1, blend_state)
                                     .set_raster_state(
                                         {
                                             .cull_mode = cull_mode,
                                             .front_face = front_face
                                         }
                                     )
                                     .build();
    proxy.pipelines[ScenePassType::RSM] = rsm_pipeline;

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
    proxy.pipelines[ScenePassType::Shadow] = shadow_pipeline;

    // Voxelization
    auto voxel_blend_state = blend_state;
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
    proxy.pipelines[ScenePassType::Vozelization] = voxelization_pipeline;

    if(backend.supports_device_generated_commands()) {
        ZoneScopedN("Create VkPipelines");
        [[maybe_unused]] const auto forced_depth_prepass_pipeline = backend
                                                                    .get_pipeline_cache()
                                                                    .get_pipeline_for_dynamic_rendering(
                                                                        depth_prepass_pipeline,
                                                                        {},
                                                                        VK_FORMAT_D32_SFLOAT,
                                                                        0);
        [[maybe_unused]] const auto forced_gbuffer_pipeline = backend
                                                              .get_pipeline_cache()
                                                              .get_pipeline_for_dynamic_rendering(
                                                                  gbuffer_pipeline,
                                                                  std::array{
                                                                      VK_FORMAT_R8G8B8A8_SRGB,
                                                                      VK_FORMAT_R16G16B16A16_SFLOAT,
                                                                      VK_FORMAT_R8G8B8A8_UNORM,
                                                                      VK_FORMAT_R8G8B8A8_SRGB
                                                                  },
                                                                  VK_FORMAT_D32_SFLOAT,
                                                                  0);
        [[maybe_unused]] const auto forced_rsm_pipeline = backend
                                                          .get_pipeline_cache()
                                                          .get_pipeline_for_dynamic_rendering(
                                                              rsm_pipeline,
                                                              std::array{
                                                                  VK_FORMAT_R8G8B8A8_SRGB,
                                                                  VK_FORMAT_R8G8B8A8_UNORM,
                                                              },
                                                              VK_FORMAT_D16_UNORM,
                                                              0);
        [[maybe_unused]] const auto forced_shadow_pipeline = backend
                                                             .get_pipeline_cache()
                                                             .get_pipeline_for_dynamic_rendering(
                                                                 shadow_pipeline,
                                                                 {},
                                                                 VK_FORMAT_D16_UNORM,
                                                                 0);
        [[maybe_unused]] const auto forced_voxelization_pipeline = backend
                                                                   .get_pipeline_cache()
                                                                   .get_pipeline_for_dynamic_rendering(
                                                                       voxelization_pipeline,
                                                                       {},
                                                                       VK_FORMAT_D32_SFLOAT,
                                                                       0);
    }

    is_pipeline_group_dirty = true;

    return handle;
}

void MaterialStorage::destroy_material(PooledObject<BasicPbrMaterialProxy>&& proxy) {
    material_pool.free_object(proxy);
}

void MaterialStorage::flush_material_buffer(RenderGraph& graph) {
    material_upload.flush_to_buffer(graph, material_buffer_handle);
}

BufferHandle MaterialStorage::get_material_buffer() const {
    return material_buffer_handle;
}

GraphicsPipelineHandle MaterialStorage::get_pipeline_group() {
    if(!is_pipeline_group_dirty & pipeline_group.is_valid()) {
        return pipeline_group;
    }

    auto& backend = RenderBackend::get();
    auto& pipeline_allocator = backend.get_pipeline_cache();

    auto pipelines_in_group = std::vector<GraphicsPipelineHandle>{};
    pipelines_in_group.reserve(1024);
    const auto& material_proxies = material_pool.get_data();
    for(auto i = 0; i < material_proxies.size(); i++) {
        const auto& proxy = material_pool.make_handle(i);
        pipelines_in_group.insert(
            pipelines_in_group.end(),
            proxy->second.pipelines.begin(),
            proxy->second.pipelines.end());
    }

    pipeline_group = pipeline_allocator.create_pipeline_group(pipelines_in_group);

    is_pipeline_group_dirty = false;

    return pipeline_group;
}
