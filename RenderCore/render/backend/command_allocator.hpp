#pragma once

#include <vector>

#include <volk.h>
#include <tl/optional.hpp>

class CommandAllocator {
public:
    /**
     * Creates a new command allocator
     *
     * @param device
     * @return
     */
    static tl::optional<CommandAllocator> create(VkDevice device, uint32_t queue_index);

    /**
     * Allocates a command buffer
     *
     * If there's free command buffers available, returns one of those. If not, allocates a new one
     *
     * @return
     */
    VkCommandBuffer allocate_command_buffer();

    /**
     * Returns a command buffer to the pool
     */
    void return_command_buffer(VkCommandBuffer buffer);

    void reset();

    // Default constructor do not use
    CommandAllocator() = default;

private:
    VkDevice device = VK_NULL_HANDLE;

    VkCommandPool command_pool = VK_NULL_HANDLE;

    std::vector<VkCommandBuffer> command_buffers;

    std::vector<VkCommandBuffer> available_command_buffers;

    CommandAllocator(VkDevice device_in);
};



