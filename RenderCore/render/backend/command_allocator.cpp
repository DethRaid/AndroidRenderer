#include "command_allocator.hpp"

#include <spdlog/sinks/android_sink.h>
#include <spdlog/logger.h>
#include <vulkan/vk_enum_string_helper.h>

#include "render_backend.hpp"
#include "core/system_interface.hpp"

static std::shared_ptr<spdlog::logger> logger;

CommandAllocator::CommandAllocator(RenderBackend& backend_in, const uint32_t queue_index) : backend{ &backend_in } {
    if (logger == nullptr) {
        logger = SystemInterface::get().get_logger("CommandAllocator");
    }
    const auto create_info = VkCommandPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = queue_index,
    };

    const auto& device = backend->get_device();
    const auto result = vkCreateCommandPool(device, &create_info, nullptr, &command_pool);
    if (result != VK_SUCCESS) {
        logger->error("Could not create command pool: Vulkan error {}", result);
        throw std::runtime_error{"Could not create command pool"};
    }
    const auto name = fmt::format("Command allocator for queue family {}", queue_index);
    backend->set_object_name(command_pool, name);
}

CommandAllocator::CommandAllocator(CommandAllocator&& old) noexcept : backend{old.backend },
                                                                      command_pool{old.command_pool},
                                                                      command_buffers{std::move(old.command_buffers)},
                                                                      available_command_buffers{
                                                                          std::move(old.command_buffers)
                                                                      } {
    old.command_pool = VK_NULL_HANDLE;
}

CommandAllocator& CommandAllocator::operator=(CommandAllocator&& old) noexcept {
    backend = old.backend;
    command_pool = old.command_pool;
    command_buffers = std::move(old.command_buffers);
    available_command_buffers = std::move(old.command_buffers);

    old.command_pool = VK_NULL_HANDLE;

    return *this;
}

CommandAllocator::~CommandAllocator() {
    const auto& device = backend->get_device();
    if (command_pool != VK_NULL_HANDLE) {
        if (!command_buffers.empty()) {
            vkFreeCommandBuffers(
                device, command_pool, static_cast<uint32_t>(command_buffers.size()), command_buffers.data()
            );
        }
        if (!available_command_buffers.empty()) {
            vkFreeCommandBuffers(
                device, command_pool, static_cast<uint32_t>(available_command_buffers.size()),
                available_command_buffers.data()
            );
        }

        vkDestroyCommandPool(device, command_pool, nullptr);

        command_pool = VK_NULL_HANDLE;
    }
}

VkCommandBuffer CommandAllocator::allocate_command_buffer(const std::string& name) {
    if (!available_command_buffers.empty()) {
        auto commands = available_command_buffers.back();
        available_command_buffers.pop_back();
        return commands;
    }

    const auto alloc_info = VkCommandBufferAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    const auto& device = backend->get_device();
    auto commands = VkCommandBuffer{};
    const auto result = vkAllocateCommandBuffers(device, &alloc_info, &commands);
    if (result != VK_SUCCESS) {
        throw std::runtime_error{fmt::format("Could not allocate command buffer: Vulkan error {}", result)};
    }

    backend->set_object_name(commands, name);
    
    return commands;
}

void CommandAllocator::return_command_buffer(const VkCommandBuffer buffer) {
    command_buffers.push_back(buffer);
}

void CommandAllocator::reset() {
    const auto& device = backend->get_device();
    const auto result = vkResetCommandPool(device, command_pool, 0);
    if(result != VK_SUCCESS) {
        logger->error("Resetting command pool failed: {}", string_VkResult(result));
    }

    available_command_buffers.insert(available_command_buffers.end(), command_buffers.begin(), command_buffers.end());

    command_buffers.clear();
}
