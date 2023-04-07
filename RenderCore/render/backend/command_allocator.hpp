#pragma once

#include <vector>

#include <volk.h>

class CommandAllocator {
public:
    CommandAllocator() = default;

    CommandAllocator(VkDevice device_in, uint32_t queue_index);

    CommandAllocator(const CommandAllocator& other) = delete;
    CommandAllocator& operator=(const CommandAllocator& other) = delete;

    CommandAllocator(CommandAllocator&& old) noexcept;
    CommandAllocator& operator=(CommandAllocator&& old) noexcept;

    ~CommandAllocator();
    
    /**
     * Allocates a command buffer
     *
     * If there's free command buffers available, returns one of those. If not, allocates a new one
     */
    VkCommandBuffer allocate_command_buffer();

    /**
     * Returns a command buffer to the pool
     */
    void return_command_buffer(VkCommandBuffer buffer);

    void reset();

private:
    VkDevice device = VK_NULL_HANDLE;

    VkCommandPool command_pool = VK_NULL_HANDLE;

    std::vector<VkCommandBuffer> command_buffers;

    std::vector<VkCommandBuffer> available_command_buffers;
};



