#pragma once

#include <string>

#include <vk_mem_alloc.h>
#include "render/backend/device_address.hpp"

struct GpuBuffer {
    std::string name;

    VkBufferCreateInfo create_info;

    VkBuffer buffer = VK_NULL_HANDLE;

    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocation_info = {};

    /**
     * \brief Device address of this buffer
     *
     * This is set to 0 for uniform buffers. We still bind uniform buffers with descriptors
     */
    DeviceAddress address;

    bool operator==(const GpuBuffer& other) const;
};

inline bool GpuBuffer::operator==(const GpuBuffer& other) const {
    return memcmp(this, &other, sizeof(GpuBuffer)) == 0;
}
