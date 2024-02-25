#pragma once

#include "render/backend/buffer_usage_token.hpp"
#include "render/backend/texture_usage_token.hpp"
#include "render/backend/handles.hpp"

class RenderBackend;

struct DescriptorInfo : VkDescriptorSetLayoutBinding {
    bool is_read_only = false;
};

struct DescriptorSetInfo {
    std::unordered_map<uint32_t, DescriptorInfo> bindings;

    bool has_variable_count_binding = false;
};

class DescriptorSet {
public:
    explicit DescriptorSet(RenderBackend& backend_in, DescriptorSetInfo set_info_in);

    DescriptorSet& bind(uint32_t binding_index, BufferHandle buffer);

    DescriptorSet& bind(uint32_t binding_index, TextureHandle texture);

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

    DescriptorSetInfo set_info;

    VkDescriptorSet descriptor_set;

    struct BoundResource {
        union {
            BufferHandle buffer;
            TextureHandle texture;
            AccelerationStructureHandle acceleration_structure;
        };
    };

    std::unordered_map<uint32_t, BoundResource> bindings;
};

