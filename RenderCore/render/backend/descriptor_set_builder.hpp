#pragma once

#include "render/backend/buffer_usage_token.hpp"
#include "render/backend/texture_usage_token.hpp"
#include "render/backend/handles.hpp"
#include "render/backend/descriptor_set_info.hpp"

class DescriptorSetAllocator;
class RenderBackend;

struct CombinedImageSampler {
    TextureHandle texture;

    VkSampler sampler;
};

class DescriptorSet {
public:
    explicit DescriptorSet(RenderBackend& backend_in, DescriptorSetAllocator& allocator_in, DescriptorSetInfo set_info_in);

    DescriptorSet& bind(uint32_t binding_index, BufferHandle buffer);

    DescriptorSet& bind(uint32_t binding_index, TextureHandle texture);

    DescriptorSet& bind(uint32_t binding_index, TextureHandle texture, VkSampler vk_sampler);

    DescriptorSet& bind(uint32_t binding_index, AccelerationStructureHandle acceleration_structure);

    /**
     * \brief Creates the Vulkan descriptor set 
     * \return 
     */
    DescriptorSet& finalize();

    void get_resource_usage_information(TextureUsageMap& texture_usages, BufferUsageMap& buffer_usages) const;

    VkDescriptorSet get_vk_descriptor_set() const;

private:
    RenderBackend& backend;

    DescriptorSetAllocator& allocator;

    DescriptorSetInfo set_info;

    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

    struct BoundResource {
        union {
            BufferHandle buffer;
            TextureHandle texture;
            CombinedImageSampler combined_image_sampler;
            AccelerationStructureHandle acceleration_structure;
        };
    };

    std::unordered_map<uint32_t, BoundResource> bindings;
};

