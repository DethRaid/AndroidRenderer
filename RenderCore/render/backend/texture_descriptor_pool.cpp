#include "texture_descriptor_pool.hpp"

#include <numeric>

#include "render/backend/render_backend.hpp"

TextureDescriptorPool::TextureDescriptorPool(const RenderBackend& backend_in) : backend{backend_in} {
    const auto device = backend_in.get_device().device;

    constexpr auto sampled_image_count = 65536u;

    constexpr auto pool_sizes = VkDescriptorPoolSize{
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = sampled_image_count,
    };
    constexpr auto pool_create_info = VkDescriptorPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_sizes,
    };
    vkCreateDescriptorPool(device, &pool_create_info, nullptr, &descriptor_pool);

    constexpr auto flags = static_cast<VkDescriptorBindingFlags>(
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    );
    constexpr auto flags_create_info = VkDescriptorSetLayoutBindingFlagsCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = 1,
        .pBindingFlags = &flags,
    };
    constexpr auto binding = VkDescriptorSetLayoutBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = sampled_image_count,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };
    constexpr auto create_info = VkDescriptorSetLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &flags_create_info,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = 1,
        .pBindings = &binding
    };
    vkCreateDescriptorSetLayout(device, &create_info, nullptr, &descriptor_set_layout);

    constexpr auto set_counts = VkDescriptorSetVariableDescriptorCountAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &sampled_image_count,
    };
    const auto allocate_info = VkDescriptorSetAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = &set_counts,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptor_set_layout,
    };
    vkAllocateDescriptorSets(device, &allocate_info, &descriptor_set);

    available_handles.resize(sampled_image_count);
    std::iota(available_handles.begin(), available_handles.end(), 0);
}

TextureDescriptorPool::~TextureDescriptorPool() {
    const auto device = backend.get_device().device;
    
    vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
    vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
}

uint32_t TextureDescriptorPool::create_texture_srv(const TextureHandle texture, const VkSampler sampler) {
    const auto handle = available_handles.back();
    available_handles.pop_back();

    const auto& allocator = backend.get_global_allocator();
    const auto& texture_actual = allocator.get_texture(texture);

    auto image_info = std::make_unique<VkDescriptorImageInfo>(
        VkDescriptorImageInfo{
            .sampler = sampler,
            .imageView = texture_actual.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }
    );

    const auto write = VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptor_set,
        .dstBinding = 0,
        .dstArrayElement = handle,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = image_info.get()
    };

    image_infos.emplace_back(std::move(image_info));
    pending_writes.push_back(write);

    return handle;
}

void TextureDescriptorPool::free_descriptor(const uint32_t handle) {
    available_handles.push_back(handle);
}

void TextureDescriptorPool::commit_descriptors() {
    if (pending_writes.empty()) {
        return;
    }

    ZoneScoped;

    const auto device = backend.get_device().device;
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(pending_writes.size()), pending_writes.data(), 0, nullptr);

    pending_writes.clear();
    image_infos.clear();
}

VkDescriptorSetLayout TextureDescriptorPool::get_descriptor_layout() const { return descriptor_set_layout; }

VkDescriptorSet TextureDescriptorPool::get_descriptor_set() const { return descriptor_set; }
