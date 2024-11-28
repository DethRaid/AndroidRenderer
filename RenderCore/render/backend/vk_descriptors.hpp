#pragma once

#include <vector>
#include <optional>

#include <volk.h>
#include <absl/container/flat_hash_map.h>

#include "render/backend/handles.hpp"

class RenderBackend;

namespace vkutil {
    class DescriptorAllocator {
    public:
        struct PoolSizes {
            std::vector<std::pair<VkDescriptorType, float>> sizes =
            {
                {VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f},
                {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f},
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f},
                {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f},
                {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f},
                {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f}
            };
        };

        void reset_pools();

        bool allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout, const VkDescriptorSetVariableDescriptorCountAllocateInfo* variable_count_info = nullptr);

        void init(VkDevice newDevice);

        void cleanup();

        VkDevice device;

    private:
        VkDescriptorPool grab_pool();

        VkDescriptorPool currentPool{VK_NULL_HANDLE};
        PoolSizes descriptorSizes;
        std::vector<VkDescriptorPool> usedPools;
        std::vector<VkDescriptorPool> freePools;
    };


    class DescriptorLayoutCache {
    public:
        void init(VkDevice newDevice);

        void cleanup();

        VkDescriptorSetLayout create_descriptor_layout(VkDescriptorSetLayoutCreateInfo* info);

        struct DescriptorLayoutInfo {
            //good idea to turn this into a inlined array
            std::vector<VkDescriptorSetLayoutBinding> bindings;

            bool operator==(const DescriptorLayoutInfo& other) const;

            size_t hash() const;
        };

    private:
        struct DescriptorLayoutHash {
            std::size_t operator()(const DescriptorLayoutInfo& k) const {
                return k.hash();
            }
        };

        absl::flat_hash_map<DescriptorLayoutInfo, VkDescriptorSetLayout, DescriptorLayoutHash> layoutCache;
        VkDevice device;
    };

    class DescriptorBuilder {
    public:
        struct BufferInfo {
            BufferHandle buffer = {};
            VkDeviceSize offset = {};
            VkDeviceSize range = {};
        };

        struct ImageInfo {
            VkSampler sampler = {};
            TextureHandle image = {};
            VkImageLayout image_layout = {};
            std::optional<uint32_t> mip_level = std::nullopt;
        };

        static DescriptorBuilder begin(RenderBackend& backend, vkutil::DescriptorAllocator& allocator);

        DescriptorBuilder& bind_buffer(
            uint32_t binding, const BufferInfo& info, VkDescriptorType type, VkShaderStageFlags stage_flags
        );

        DescriptorBuilder& bind_buffer_array(
            uint32_t binding, const std::vector<BufferInfo>& infos, VkDescriptorType type,
            VkShaderStageFlags stage_flags
        );

        DescriptorBuilder& bind_image(
            uint32_t binding, const ImageInfo& info, VkDescriptorType type, VkShaderStageFlags stage_flags
        );

        DescriptorBuilder& bind_image_array(
            uint32_t binding, const std::vector<ImageInfo>& infos,
            VkDescriptorType type, VkShaderStageFlags stage_flags
        );

        DescriptorBuilder& bind_buffer(
            uint32_t binding, const VkDescriptorBufferInfo* buffer_infos, VkDescriptorType type,
            VkShaderStageFlags stageFlags, uint32_t count = 1
        );

        DescriptorBuilder& bind_image(
            uint32_t binding, VkDescriptorImageInfo* imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t count = 1
        );

        std::optional<VkDescriptorSet> build(VkDescriptorSetLayout& layout);

        std::optional<VkDescriptorSet> build();

    private:
        RenderBackend& backend;

        std::vector<VkWriteDescriptorSet> writes;
        std::vector<VkDescriptorSetLayoutBinding> bindings;

        DescriptorLayoutCache& cache;
        DescriptorAllocator& alloc;

        std::vector<std::vector<VkDescriptorBufferInfo>> buffer_infos_to_delete;
        std::vector<std::vector<VkDescriptorImageInfo>> image_infos_to_delete;

        DescriptorBuilder(RenderBackend& backend_in, vkutil::DescriptorAllocator& allocator_in);
    };
}
