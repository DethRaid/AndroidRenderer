#pragma once

#include <span>
#include <string>
#include <string_view>

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
            AccelerationStructureHandle address = {};
        };
    };
}

struct DescriptorSet {
public:
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;

    DescriptorSetInfo set_info;

    std::vector<detail::BoundResource> bindings;

    void get_resource_usage_information(
        std::vector<TextureUsageToken>& texture_usages,
        std::vector<BufferUsageToken>& buffer_usages
    ) const;
};

class DescriptorSetBuilder {
public:
    explicit DescriptorSetBuilder(
        RenderBackend& backend_in, DescriptorSetAllocator& allocator_in,
        DescriptorSetInfo set_info_in, std::string_view name_in
    );

    DescriptorSetBuilder& bind(uint32_t binding_index, BufferHandle buffer);

    DescriptorSetBuilder& bind(uint32_t binding_index, TextureHandle texture);

    DescriptorSetBuilder& bind(uint32_t binding_index, TextureHandle texture, VkSampler vk_sampler);

    DescriptorSetBuilder& bind(
        uint32_t binding_index,
        AccelerationStructureHandle acceleration_structure
    );

    /**
     * \brief Creates the Vulkan descriptor set 
     * \return 
     */
    DescriptorSet build();

private:
    RenderBackend* backend;

    DescriptorSetAllocator* allocator;

    DescriptorSetInfo set_info;

    std::vector<detail::BoundResource> bindings;

    std::string name;
};
