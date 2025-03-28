#pragma once

#include <span>
#include <string>
#include <string_view>

#include <EASTL/vector.h>
#include <EASTL/fixed_vector.h>

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
            AccelerationStructureHandle acceleration_structure = {};
        };
    };
}

struct DescriptorSet {
public:
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;

    DescriptorSetInfo set_info;

    eastl::fixed_vector<detail::BoundResource, 16> bindings;

    void get_resource_usage_information(
        TextureUsageList& texture_usages,
        BufferUsageList& buffer_usages
    ) const;
};

class DescriptorSetBuilder {
public:
    explicit DescriptorSetBuilder(
        RenderBackend& backend_in, DescriptorSetAllocator& allocator_in,
        DescriptorSetInfo set_info_in, std::string_view name_in
    );

    DescriptorSetBuilder& bind(BufferHandle buffer);

    DescriptorSetBuilder& bind(TextureHandle texture);

    DescriptorSetBuilder& bind(TextureHandle texture, VkSampler vk_sampler);

    DescriptorSetBuilder& bind(AccelerationStructureHandle acceleration_structure);

    DescriptorSetBuilder& next_binding(uint32_t binding_index);

    /**
     * \brief Creates the Vulkan descriptor set 
     * \return 
     */
    DescriptorSet build();

private:
    RenderBackend* backend;

    DescriptorSetAllocator* allocator;

    DescriptorSetInfo set_info;

    uint32_t binding_index = 0;

    eastl::fixed_vector<detail::BoundResource, 16> bindings;

    std::string name;
};
