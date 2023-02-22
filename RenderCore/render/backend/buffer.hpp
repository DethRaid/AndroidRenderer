#pragma once

#include <string>

#include <volk.h>
#include <vk_mem_alloc.h>

struct Buffer {
    std::string name;
    VkBufferCreateInfo create_info;

    VkBuffer buffer = VK_NULL_HANDLE;

    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocation_info = {};
};
