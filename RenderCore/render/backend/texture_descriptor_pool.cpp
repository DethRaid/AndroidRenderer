#include "texture_descriptor_pool.hpp"

#include <numeric>

#include "console/cvars.hpp"
#include "render/backend/render_backend.hpp"

static AutoCVar_Int cvar_sampled_image_count{
    "r.RHI.SampledImageCount", "Maximum number of sampled images that the GPU can access", 65536
};

TextureDescriptorPool::TextureDescriptorPool(RenderBackend& backend_in) : backend{ backend_in } {
    auto sampled_image_count = backend.get_physical_device().properties.limits.maxDescriptorSetSampledImages;
    cvar_sampled_image_count.Set(sampled_image_count > INT_MAX ? INT_MAX : static_cast<int32_t>(sampled_image_count / 2));
    sampled_image_count = cvar_sampled_image_count.Get();

    const auto& device = backend_in.get_device();

    const auto pool_sizes = VkDescriptorPoolSize{
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = sampled_image_count,
    };
    const auto pool_create_info = VkDescriptorPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_sizes,
    };
    vkCreateDescriptorPool(device, &pool_create_info, nullptr, &descriptor_pool);

    const auto binding = VkDescriptorSetLayoutBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = sampled_image_count,
        .stageFlags = VK_SHADER_STAGE_ALL
    };
    auto create_info = VkDescriptorSetLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = 1,
        .pBindings = &binding
    };
    auto& cache = backend.get_descriptor_cache();
    descriptor_set.layout = cache.create_descriptor_layout(&create_info);

    const auto set_counts = VkDescriptorSetVariableDescriptorCountAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &sampled_image_count,
    };
    const auto allocate_info = VkDescriptorSetAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = &set_counts,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptor_set.layout,
    };
    vkAllocateDescriptorSets(device, &allocate_info, &descriptor_set.descriptor_set);

    available_handles.resize(sampled_image_count);
    std::iota(available_handles.begin(), available_handles.end(), 0);
}

TextureDescriptorPool::~TextureDescriptorPool() {
    const auto& device = backend.get_device();
    
    vkDestroyDescriptorSetLayout(device, descriptor_set.layout, nullptr);
    vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
}

uint32_t TextureDescriptorPool::create_texture_srv(const TextureHandle texture, const VkSampler sampler) {
    const auto handle = available_handles.back();
    available_handles.pop_back();

    auto image_info = std::make_unique<VkDescriptorImageInfo>(
        VkDescriptorImageInfo{
            .sampler = sampler,
            .imageView = texture->image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }
    );

    const auto write = VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptor_set.descriptor_set,
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

    const auto& device = backend.get_device();
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(pending_writes.size()), pending_writes.data(), 0, nullptr);

    pending_writes.clear();
    image_infos.clear();
}

const DescriptorSet& TextureDescriptorPool::get_descriptor_set() const { return descriptor_set; }
