#pragma once

#include "render/backend/acceleration_structure.hpp"
#include "render/backend/buffer_usage_token.hpp"
#include "render/backend/texture_usage_token.hpp"
#include "render/backend/handles.hpp"
#include "render/backend/descriptor_set_info.hpp"

class DescriptorSetAllocator;
class RenderBackend;

namespace detail {
    struct CombinedImageSampler {
        TextureHandle texture;

        VkSampler sampler;
    };

    struct BoundResource {
        union {
            BufferHandle buffer;
            TextureHandle texture;
            CombinedImageSampler combined_image_sampler;
            DeviceAddress address = {};
        };
    };
}

struct DescriptorSet {
public:
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;

    DescriptorSetInfo set_info;

    absl::flat_hash_map<uint32_t, detail::BoundResource> bindings;

    void get_resource_usage_information(TextureUsageMap& texture_usages, absl::flat_hash_map<BufferHandle, BufferUsageToken>& buffer_usages) const;
};

class DescriptorSetBuilder {
public:
    explicit DescriptorSetBuilder(RenderBackend& backend_in, DescriptorSetAllocator& allocator_in, DescriptorSetInfo set_info_in);

    DescriptorSetBuilder& bind(uint32_t binding_index, BufferHandle buffer);

    DescriptorSetBuilder& bind(uint32_t binding_index, TextureHandle texture);

    DescriptorSetBuilder& bind(uint32_t binding_index, TextureHandle texture, VkSampler vk_sampler);

    DescriptorSetBuilder& bind(uint32_t binding_index, AccelerationStructureHandle acceleration_structure);

    /**
     * \brief Creates the Vulkan descriptor set 
     * \return 
     */
    DescriptorSet build();

private:
    RenderBackend* backend;

    DescriptorSetAllocator* allocator;

    DescriptorSetInfo set_info;

    absl::flat_hash_map<uint32_t, detail::BoundResource> bindings;
};
