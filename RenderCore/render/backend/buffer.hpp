#pragma once

#include <string>

#include <glm/vec2.hpp>
#include <volk.h>
#include <vk_mem_alloc.h>

struct Buffer {
    std::string name;
    VkBufferCreateInfo create_info;

    VkBuffer buffer = VK_NULL_HANDLE;

    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocation_info = {};

    /**
     * \brief Device address of this buffer, split into low and high parts
     *
     * This is set to 0 for uniform buffers. We still bind uniform buffers with descriptors
     */
    glm::uvec2 address = {};
};
