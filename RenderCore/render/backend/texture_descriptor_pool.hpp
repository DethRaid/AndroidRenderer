#pragma once

#include <memory>
#include <vector>
#include <volk.h>

#include "render/backend/handles.hpp"

class ResourceAllocator;
class RenderBackend;
/**
 * \brief A pool for texture descriptors
 */
class TextureDescriptorPool {
public:
    explicit TextureDescriptorPool(const RenderBackend& backend_in);

    ~TextureDescriptorPool();

    uint32_t create_texture_srv(TextureHandle texture, VkSampler sampler);

    void free_descriptor(uint32_t handle);

    /**
     * \brief Commits pending descriptor writes
     *
     * Should be called at start of frame
     */
    void commit_descriptors();

    VkDescriptorSetLayout get_descriptor_layout() const;

    VkDescriptorSet get_descriptor_set() const;

private:
    const RenderBackend& backend;

    VkDescriptorPool descriptor_pool;

    VkDescriptorSetLayout descriptor_set_layout;

    VkDescriptorSet descriptor_set;

    std::vector<uint32_t> available_handles;

    std::vector<std::unique_ptr<VkDescriptorImageInfo>> image_infos;
    std::vector<VkWriteDescriptorSet> pending_writes;
};
