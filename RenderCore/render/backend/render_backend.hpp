#ifndef SAHRENDERER_RENDER_BACKEND_HPP
#define SAHRENDERER_RENDER_BACKEND_HPP

#include <array>

#include <volk.h>
#include <VkBootstrap.h>
#include <tracy/TracyVulkan.hpp>

#include "render/backend/resource_allocator.hpp"
#include "core/object_pool.hpp"
#include "render/backend/pipeline.hpp"
#include "render/backend/command_allocator.hpp"
#include "render/backend/resource_upload_queue.hpp"
#include "render/backend/command_buffer.hpp"
#include "render/backend/constants.hpp"
#include "render/backend/buffer_usage_token.hpp"
#include "render/backend/compute_shader.hpp"

/**
 * Wraps a lot of low level Vulkan concerns
 *
 * The render backend is very stateful. It knows the current frame index, it has a command buffer for your use, etc
 *
 * The command buffer is submitted and replaced with a new command buffer when you flush commands. We automatically
 * flush commands at the end of the frame, but you can flush more often to reduce latency
 *
 * When the frame begins, the backend does a couple things:
 * - Wait for the GPU to finish processing the last commands for the current frame index
 * - Destroys the command buffers for this frame and resets the command pool
 */
class RenderBackend {
public:
    explicit RenderBackend();

    RenderBackend(const RenderBackend& other) = delete;

    RenderBackend& operator=(const RenderBackend& other) = delete;

    RenderBackend(RenderBackend&& old) noexcept = default;

    RenderBackend& operator=(RenderBackend&& old) noexcept = default;

    ~RenderBackend();

    VkInstance get_instance() const;

    VkPhysicalDevice get_physical_device() const;

    vkb::Device get_device() const;

    uint32_t get_graphics_queue_index() const;

    VkQueue get_transfer_queue() const;

    uint32_t get_transfer_queue_family_index() const;

    PipelineBuilder begin_building_pipeline(std::string_view name) const;

    tl::optional<ComputeShader>
    create_compute_shader(const std::string& name, const std::vector<uint8_t>& instructions) const;

    uint32_t get_current_gpu_frame() const;

    /**
     * Begins the frame
     *
     * Waits for the GPU to finish with this frame, does some beginning-of-frame setup, is generally cool
     */
    void begin_frame();

    /**
     * Submits queued resource uploads, submits batched command buffers, presents the swapchain
     */
    void end_frame();

    void collect_tracy_data(CommandBuffer& commands);

    TracyVkCtx get_tracy_context();

    ResourceAllocator& get_global_allocator();

    ResourceUploadQueue& get_upload_queue();

    /**
     * Creates a descriptor builder for descriptors that will persist for a while
     *
     * Callers should save and re-used these descriptors
     */
    vkutil::DescriptorBuilder create_persistent_descriptor_builder();

    /**
     * Creates a descriptor builder for descriptors that can be blown away after this frame
     *
     * Callers should make no effort to save these descriptors
     */
    vkutil::DescriptorBuilder create_frame_descriptor_builder();

    CommandBuffer create_command_buffer();

    /**
     * Creates a command buffer that can transfer data around. Intended to be used internally by backend subsystems,
     * frontend systems should use one of the abstractions
     *
     * This command buffer may or may not be for a dedicated transfer queue.
     *
     * The user must begin the command buffer themselves
     *
     * @return A command buffer that can transfer data
     */
    VkCommandBuffer create_transfer_command_buffer();

    /**
     * Submits a command buffer to the backend
     *
     * The backend will queue up command buffers and submit them at end-of-frame
     *
     * The command buffers must be ended by the user
     *
     * @param commands
     */
    void submit_transfer_command_buffer(VkCommandBuffer commands);

    void submit_command_buffer(CommandBuffer&& commands);

    vkb::Swapchain& get_swapchain();

    uint32_t get_current_swapchain_index() const;

    VkPipelineCache get_pipeline_cache() const;

    vkutil::DescriptorLayoutCache& get_descriptor_cache();

    TextureHandle get_white_texture_handle() const;

    TextureHandle get_default_normalmap_handle() const;

    VkSampler get_default_sampler() const;

private:
    uint32_t cur_frame_idx = 0;

    vkb::Instance instance;

    VkSurfaceKHR surface = {};

    vkb::PhysicalDevice physical_device;
    vkb::Device device;

    VkQueue graphics_queue;
    uint32_t graphics_queue_family_index;

    tl::optional<VkQueue> transfer_queue = tl::nullopt;
    tl::optional<uint32_t> transfer_queue_family_index = tl::nullopt;

    std::unique_ptr<ResourceAllocator> allocator;

    std::unique_ptr<ResourceUploadQueue> upload_queue = {};

    VkCommandPool tracy_command_pool = VK_NULL_HANDLE;
    VkCommandBuffer tracy_command_buffer = VK_NULL_HANDLE;
    TracyVkCtx tracy_context = nullptr;

    vkutil::DescriptorAllocator global_descriptor_allocator;

    std::array<vkutil::DescriptorAllocator, num_in_flight_frames> frame_descriptor_allocators;

    vkutil::DescriptorLayoutCache descriptor_layout_cache;

    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;

    TextureHandle white_texture_handle = TextureHandle::None;
    TextureHandle default_normalmap_handle = TextureHandle::None;
    VkSampler default_sampler = VK_NULL_HANDLE;

    vkb::Swapchain swapchain = {};

    /**
     * Semaphore signaled by the most recent swapchain image acquire operation
     */
    VkSemaphore swapchain_semaphore = VK_NULL_HANDLE;

    uint32_t cur_swapchain_image_idx = 0;

    std::array<VkFence, num_in_flight_frames> frame_fences = {};

    std::array<CommandAllocator, num_in_flight_frames> command_allocators = {};

    std::vector<VkCommandBuffer> queued_transfer_command_buffers = {};

    std::vector<CommandBuffer> queued_command_buffers = {};
    
    void create_instance_and_device();

    void create_swapchain();

    void create_tracy_context();

    void create_command_pools();

    /**
     * Creates a semaphore that'll be destroyed at the start of next frame
     *
     * @return A semaphore that's only valid until the start of the next frame with the current frame's index
     */
    VkSemaphore create_transient_semaphore();

    void destroy_semaphore(VkSemaphore semaphore);

    std::array<std::vector<VkSemaphore>, num_in_flight_frames> zombie_semaphores;
    std::vector<VkSemaphore> available_semaphores;

    void create_pipeline_cache();

    void create_default_resources();
};

#endif //SAHRENDERER_RENDER_BACKEND_HPP
