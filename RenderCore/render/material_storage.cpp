#include "material_storage.hpp"
#include "render/backend/render_backend.hpp"

MaterialStorage::MaterialStorage() : basic_bpr_material_pipelines{ "gltf_basic_pbr" } {
    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();
    material_instance_buffer_handle = allocator.create_buffer(
        "Materials buffer",
        sizeof(BasicPbrMaterialGpu) * 65536,
        BufferUsage::StorageBuffer
    );
}

PooledObject<BasicPbrMaterialProxy> MaterialStorage::add_material_instance(BasicPbrMaterial&& new_material) {
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

    const auto handle = material_instance_pool.add_object(std::make_pair(new_material, MaterialProxy{}));

    material_instance_upload_buffer.add_data(handle.index, new_material.gpu_data);

    return handle;
}

void MaterialStorage::destroy_material_instance(PooledObject<BasicPbrMaterialProxy>&& proxy) {
    material_instance_pool.free_object(proxy);
}

void MaterialStorage::flush_material_instance_buffer(RenderGraph& graph) {
    material_instance_upload_buffer.flush_to_buffer(graph, material_instance_buffer_handle);
}

BufferHandle MaterialStorage::get_material_instance_buffer() const {
    return material_instance_buffer_handle;
}

GraphicsPipelineHandle MaterialStorage::get_pipeline_group() {
    if(!is_pipeline_group_dirty && pipeline_group) {
        return pipeline_group;
    }

    auto& backend = RenderBackend::get();
    auto& pipeline_allocator = backend.get_pipeline_cache();

    auto pipelines_in_group = eastl::vector<GraphicsPipelineHandle>{};
    pipelines_in_group.reserve(1024);
    const auto& material_proxies = material_instance_pool.get_data();
    for(auto i = 0; i < material_proxies.size(); i++) {
        const auto& proxy = material_instance_pool.make_handle(i);
        for(auto handle : proxy->second.pipelines) {
            pipelines_in_group.emplace_back(handle);
        }
    }

    pipeline_group = pipeline_allocator.create_pipeline_group(pipelines_in_group);

    is_pipeline_group_dirty = false;

    return pipeline_group;
}

const MaterialPipelines& MaterialStorage::get_pipelines() const { return basic_bpr_material_pipelines; }
