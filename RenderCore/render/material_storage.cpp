#include "material_storage.hpp"
#include "render/backend/render_backend.hpp"

MaterialStorage::MaterialStorage(RenderBackend& backend_in) : backend{backend_in} {
    auto& allocator = backend.get_global_allocator();
    material_buffer_handle = allocator.create_buffer("Materials buffer", sizeof(BasicPbrMaterialGpu) * 65536,
                                                     BufferUsage::UniformBuffer);
}

PooledObject<BasicPbrMaterialProxy> MaterialStorage::add_material(BasicPbrMaterial&& new_material) {
    auto builder = backend.create_persistent_descriptor_builder();
    const auto& resources = backend.get_global_allocator();

    auto proxy = MaterialProxy{};

    const auto& base_color_texture = resources.get_texture(new_material.base_color_texture);
    auto base_color_info = VkDescriptorImageInfo{
            .sampler = new_material.base_color_sampler,
            .imageView = base_color_texture.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    builder.bind_image(0, &base_color_info, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

    const auto& normal_texture = resources.get_texture(new_material.normal_texture);
    auto normal_texture_info = VkDescriptorImageInfo{
            .sampler = new_material.normal_sampler,
            .imageView = normal_texture.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    builder.bind_image(1, &normal_texture_info, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);

    const auto& metallic_roughness_texture = resources.get_texture(new_material.metallic_roughness_texture);
    auto metallic_roughness_texture_info = VkDescriptorImageInfo{
            .sampler = new_material.metallic_roughness_sampler,
            .imageView = metallic_roughness_texture.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    builder.bind_image(2, &metallic_roughness_texture_info, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);

    const auto& emission_texture = resources.get_texture(new_material.emission_texture);
    auto emission_texture_info = VkDescriptorImageInfo{
            .sampler = new_material.emission_sampler,
            .imageView = emission_texture.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    builder.bind_image(3, &emission_texture_info, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);

    const auto handle = materials.add_object(std::make_pair(new_material, proxy));

    auto& material_buffer = resources.get_buffer(material_buffer_handle);
    auto* materials_pointer = reinterpret_cast<BasicPbrMaterialGpu*>(material_buffer.allocation_info.pMappedData);
    auto& material_pointer = materials_pointer[handle.index];
    std::memcpy(&material_pointer, &new_material.gpu_data, sizeof(BasicPbrMaterialGpu));

    auto uniform_info = VkDescriptorBufferInfo{
            .buffer = material_buffer.buffer,
            .offset = handle.index * sizeof(BasicPbrMaterialGpu),
            .range = sizeof(BasicPbrMaterialGpu)
    };
    builder.bind_buffer(4, &uniform_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);

    builder.build(handle->second.descriptor_set);

    return handle;
}

void MaterialStorage::flush_material_buffer(CommandBuffer& commands) {
    commands.flush_buffer(material_buffer_handle);
}
