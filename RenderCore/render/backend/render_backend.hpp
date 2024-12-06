#pragma once

#include <array>

#include <volk.h>
#include <VkBootstrap.h>
#include <tracy/TracyVulkan.hpp>

#include "render/backend/descriptor_set_allocator.hpp"
#include "render/backend/render_graph.hpp"
#include "render/backend/resource_access_synchronizer.hpp"
#include "render/backend/texture_descriptor_pool.hpp"
#include "render/backend/resource_allocator.hpp"
#include "render/backend/command_allocator.hpp"
#include "render/backend/pipeline_builder.hpp"
#include "render/backend/resource_upload_queue.hpp"
#include "render/backend/constants.hpp"

class BlasBuildQueue;
class PipelineCache;
/**
 * Wraps a lot of low level Vulkan concerns
 *
 * The render backend is very stateful. It knows the current frame index, it has a command buffer for your use, etc.
 *
 * The command buffer is submitted and replaced with a new command buffer when you flush commands. We automatically
 * flush commands at the end of the frame, but you can flush more often to reduce latency
 *
 * When the frame begins, the backend does a couple of things:
 * - Wait for the GPU to finish processing the last commands for the current frame index
 * - Destroys the command buffers for this frame and resets the command pool
 */
class RenderBackend {
public:
    static RenderBackend& get();

    bool supports_nv_diagnostics_config = false;

    bool supports_nv_shader_reorder = false;

    explicit RenderBackend();

    RenderBackend(const RenderBackend& other) = delete;

    RenderBackend& operator=(const RenderBackend& other) = delete;

    RenderBackend(RenderBackend&& old) noexcept = default;

    RenderBackend& operator=(RenderBackend&& old) noexcept = default;

    ~RenderBackend();

    bool supports_ray_tracing() const;

    bool supports_device_generated_commands() const;

    RenderGraph create_render_graph();

    void execute_graph(RenderGraph&& render_graph);

    VkInstance get_instance() const;

    const vkb::PhysicalDevice& get_physical_device() const;

    bool supports_astc() const;

    bool supports_etc2() const;

    bool supports_bc() const;

    const vkb::Device& get_device() const;

    bool has_separate_transfer_queue() const;

    uint32_t get_graphics_queue_family_index() const;

    VkQueue get_transfer_queue() const;

    uint32_t get_transfer_queue_family_index() const;

    void add_transfer_barrier(const VkImageMemoryBarrier2& barrier);

    GraphicsPipelineBuilder begin_building_pipeline(std::string_view name) const;

    uint32_t get_current_gpu_frame() const;

    /**
     * Begins the frame
     *
     * Waits for the GPU to finish with this frame, does some beginning-of-frame setup, is generally cool
     */
    void advance_frame();

    /**
     * Flushes all batched command buffers to their respective queues
     */
    void flush_batched_command_buffers();
    
    void collect_tracy_data(const CommandBuffer& commands) const;

    TracyVkCtx get_tracy_context() const;

    ResourceAllocator& get_global_allocator() const;

    ResourceUploadQueue& get_upload_queue() const;

    BlasBuildQueue& get_blas_build_queue() const;

    ResourceAccessTracker& get_resource_access_tracker();

    PipelineCache& get_pipeline_cache() const;

    TextureDescriptorPool& get_texture_descriptor_pool() const;

    /**
     * Creates a descriptor builder for descriptors that will persist for a while
     *
     * Callers should save and re-used these descriptors
     */
    DescriptorSetAllocator& get_persistent_descriptor_allocator();

    /**
     * Creates a descriptor builder for descriptors that can be blown away after this frame
     *
     * Callers should make no effort to save these descriptors
     */
    DescriptorSetAllocator& get_transient_descriptor_allocator();

    CommandBuffer create_graphics_command_buffer(const std::string& name);

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
    VkCommandBuffer create_transfer_command_buffer(const std::string& name);
    
    /**
     * Submits a command buffer to the backend
     *
     * The backend will queue up command buffers and submit them at end-of-frame
     *
     * The command buffers must be ended by the user
     */
    void submit_transfer_command_buffer(VkCommandBuffer commands);

    void submit_command_buffer(CommandBuffer&& commands);

    /**
     * Presents the current swapchain image in the main queue
     *
     * The caller is responsible for synchronizing access to the swapchain image. See RenderGraph::add_present_pass
     */
    void present();

    vkb::Swapchain& get_swapchain();

    uint32_t get_current_swapchain_index() const;
    
    vkutil::DescriptorLayoutCache& get_descriptor_cache();

    TextureHandle get_white_texture_handle() const;

    TextureHandle get_default_normalmap_handle() const;

    VkSampler get_default_sampler() const;

    template<typename VulkanType>
    void set_object_name(VulkanType object, const std::string& name) const;

private:
    static inline std::unique_ptr<RenderBackend> g_render_backend = nullptr;

    bool is_first_frame = true;

    uint32_t cur_frame_idx = 0;
    uint64_t total_num_frames = 0;

    vkb::Instance instance;

    VkSurfaceKHR surface = {};

    vkb::PhysicalDevice physical_device;
    vkb::Device device;

    bool supports_rt = false;

    bool supports_dgc = false;

    VkQueue graphics_queue;
    uint32_t graphics_queue_family_index;

    VkQueue transfer_queue;
    uint32_t transfer_queue_family_index;

    std::unique_ptr<ResourceAllocator> allocator;

    std::unique_ptr<ResourceUploadQueue> upload_queue = {};

    std::unique_ptr<BlasBuildQueue> blas_build_queue = {};

    ResourceAccessTracker resource_access_synchronizer;

    std::unique_ptr<PipelineCache> pipeline_cache = {};

    std::unique_ptr<TextureDescriptorPool> texture_descriptor_pool = {};

    VkCommandPool tracy_command_pool = VK_NULL_HANDLE;
    VkCommandBuffer tracy_command_buffer = VK_NULL_HANDLE;
    TracyVkCtx tracy_context = nullptr;

    DescriptorSetAllocator global_descriptor_allocator;

    std::array<DescriptorSetAllocator, num_in_flight_frames> frame_descriptor_allocators;

    vkutil::DescriptorLayoutCache descriptor_layout_cache;

    VkPipelineCache vk_pipeline_cache = VK_NULL_HANDLE;

    TextureHandle white_texture_handle = nullptr;
    TextureHandle default_normalmap_handle = nullptr;
    VkSampler default_sampler = VK_NULL_HANDLE;

    vkb::Swapchain swapchain = {};

    /**
     * Semaphore signaled by the most recent swapchain image acquire operation
     */
    VkSemaphore swapchain_semaphore = VK_NULL_HANDLE;

    /**
     * Semaphore that the last command buffer submission signals
     *
     * Set to VK_NULL_HANDLE after presenting - vkQueuePresentKHR consumes the semaphore value and makes it unsignalled
     */
    std::vector<VkSemaphore> last_submission_semaphores = {};

    uint32_t cur_swapchain_image_idx = 0;

    std::array<VkFence, num_in_flight_frames> frame_fences = {};

    std::array<CommandAllocator, num_in_flight_frames> graphics_command_allocators = {};

    std::array<CommandAllocator, num_in_flight_frames> transfer_command_allocators = {};

    std::vector<VkCommandBuffer> queued_transfer_command_buffers = {};

    /**
     * Barriers that need to execute to transfer resources from the transfer queue to the graphics queue
     */
    std::vector<VkImageMemoryBarrier2> transfer_barriers = {};

    std::vector<CommandBuffer> queued_command_buffers = {};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_pipeline_features = {};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features = {};
    VkPhysicalDeviceRayQueryFeaturesKHR ray_query_features = {};
    VkPhysicalDeviceDeviceGeneratedCommandsFeaturesNV device_generated_commands_features;
    VkPhysicalDeviceFeatures2 device_features = {};

    void create_instance_and_device();

    void query_physical_device_features();

    void create_swapchain();

    void create_tracy_context();

    void create_command_pools();

    /**
     * Creates a semaphore that'll be destroyed at the start of next frame
     *
     * @return A semaphore that's only valid until the start of the next frame with the current frame's index
     */
    VkSemaphore create_transient_semaphore(const std::string& name );

    void destroy_semaphore(VkSemaphore semaphore);

    std::array<std::vector<VkSemaphore>, num_in_flight_frames> zombie_semaphores;
    std::vector<VkSemaphore> available_semaphores;
    
    void create_default_resources();

    void set_object_name(uint64_t object_handle, VkObjectType object_type, const std::string& name) const;
};


template <typename VulkanType>
void RenderBackend::set_object_name(VulkanType object, const std::string& name) const {
    auto object_type = VK_OBJECT_TYPE_UNKNOWN;
    if constexpr (std::is_same_v<VulkanType, VkImage>) {
        object_type = VK_OBJECT_TYPE_IMAGE;
    } else if constexpr (std::is_same_v<VulkanType, VkImageView>) {
        object_type = VK_OBJECT_TYPE_IMAGE_VIEW;
    } else if constexpr (std::is_same_v<VulkanType, VkBuffer>) {
        object_type = VK_OBJECT_TYPE_BUFFER;
    } else if constexpr (std::is_same_v<VulkanType, VkRenderPass>) {
        object_type = VK_OBJECT_TYPE_RENDER_PASS;
    } else if constexpr (std::is_same_v<VulkanType, VkPipeline>) {
        object_type = VK_OBJECT_TYPE_PIPELINE;
    } else if constexpr (std::is_same_v<VulkanType, VkPipelineLayout>) {
        object_type = VK_OBJECT_TYPE_PIPELINE_LAYOUT;
    } else if constexpr (std::is_same_v<VulkanType, VkShaderModule>) {
        object_type = VK_OBJECT_TYPE_SHADER_MODULE;
    } else if constexpr (std::is_same_v<VulkanType, VkQueue>) {
        object_type = VK_OBJECT_TYPE_QUEUE;
    } else if constexpr (std::is_same_v<VulkanType, VkCommandPool>) {
        object_type = VK_OBJECT_TYPE_COMMAND_POOL;
    } else if constexpr (std::is_same_v<VulkanType, VkCommandBuffer>) {
        object_type = VK_OBJECT_TYPE_COMMAND_BUFFER;
    } else {
        throw std::runtime_error{ "Invalid object type" };
    }

    set_object_name(reinterpret_cast<uint64_t>(object), object_type, name);
}
