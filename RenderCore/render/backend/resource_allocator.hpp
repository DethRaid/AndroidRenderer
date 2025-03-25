#pragma once

#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <EASTL/unordered_map.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <plf_colony.h>

#include "extern/cityhash/city_hash.hpp"
#include "core/object_pool.hpp"
#include "render/backend/acceleration_structure.hpp"
#include "render/backend/gpu_texture.hpp"
#include "render/backend/handles.hpp"
#include "render/backend/buffer.hpp"
#include "render/backend/constants.hpp"
#include "render/backend/framebuffer.hpp"

struct RenderPass;
class RenderBackend;

/**
 * How a texture might be used
 */
enum class TextureUsage {
    /**
     * The texture will be rendered to by the rasterizer. It may be sampled
     */
    RenderTarget,

    /**
     * The texture will have static data uploaded from disk. It may be sampled
     */
    StaticImage,

    /**
     * The texture will be used as a storage image. It may be sampled
     */
    StorageImage,

    /**
     * The texture will be used as a shading rate image 
     */
    ShadingRateImage,
};

inline const char* to_string(const TextureUsage e) {
    switch(e) {
    case TextureUsage::RenderTarget: return "RenderTarget";
    case TextureUsage::StaticImage: return "StaticImage";
    case TextureUsage::StorageImage: return "StorageImage";
    case TextureUsage::ShadingRateImage: return "ShadingRateImage";
    default: return "unknown";
    }
}

/**
 * How a buffer might be used
 */
enum class BufferUsage {
    /**
     * CPU writes to the buffer, it's copied to another resource
     */
    StagingBuffer,

    /**
     * Vertex buffer. Can copy vertices to it and use it for rendering
     */
    VertexBuffer,

    /**
     * Index buffer. Can copy indices to it and use it for rendering
     */
    IndexBuffer,

    /**
     * Indirect commands buffer. Written to by one shader, used as indirect dispatch or draw arguments
     */
    IndirectBuffer,

    /**
     * Uniform buffer. Persistently mapped so the CPU can write to it whenever. Be careful with synchronizing these
     */
    UniformBuffer,

    /**
     * Storage buffer. Can be copied to, written to by a shader, or read from by a shader
     */
    StorageBuffer,

    /**
     * Ray tracing acceleration structure
     */
    AccelerationStructure,

    /**
     * Shader binding table, useful for ray tracing 
     */
    ShaderBindingTable,
};

inline const char* to_string(const BufferUsage e) {
    switch(e) {
    case BufferUsage::StagingBuffer: return "StagingBuffer";
    case BufferUsage::VertexBuffer: return "VertexBuffer";
    case BufferUsage::IndexBuffer: return "IndexBuffer";
    case BufferUsage::IndirectBuffer: return "IndirectBuffer";
    case BufferUsage::UniformBuffer: return "UniformBuffer";
    case BufferUsage::StorageBuffer: return "StorageBuffer";
    case BufferUsage::AccelerationStructure: return "AccelerationStructure";
    case BufferUsage::ShaderBindingTable: return "ShaderBindingTable";
    default: return "unknown";
    }
}

struct TextureCreateInfo {
    VkFormat format = VK_FORMAT_UNDEFINED;

    glm::uvec2 resolution = {};

    uint32_t num_mips = 1;

    TextureUsage usage = TextureUsage::StaticImage;

    uint32_t num_layers = 1;

    VkFormat view_format = VK_FORMAT_UNDEFINED;

    VkImageCreateFlags flags = 0;

    VkImageUsageFlags usage_flags = 0;
};

/**
 * Allocates all kinds of resources
 *
 * When you use this class to delete a resource the resource isn't deleted immediately. Rather, it's added to a queue
 * that gets flushed at the start of the next frame
 */
class ResourceAllocator {
public:
    explicit ResourceAllocator(RenderBackend& backend_in);

    ~ResourceAllocator();

    /**
     * Creates a texture with the given parameters
     *
     * @param name Name of the texture
     * @param create_info Information about how to create the texture
     * @return A handle to the texture
     */
    TextureHandle create_texture(const std::string& name, const TextureCreateInfo& create_info);

    TextureHandle create_volume_texture(
        const std::string& name, VkFormat format, glm::uvec3 resolution, uint32_t num_mips, TextureUsage usage
    );

    TextureHandle emplace_texture(GpuTexture&& new_texture);

    void destroy_texture(TextureHandle handle);

    BufferHandle create_buffer(const std::string& name, size_t size, BufferUsage usage);

    void* map_buffer(BufferHandle buffer_handle) const;

    template <typename MappedType>
    MappedType* map_buffer(BufferHandle buffer);

    void* map_buffer(BufferHandle buffer_handle);

    AccelerationStructureHandle create_acceleration_structure();

    void destroy_buffer(BufferHandle handle);

    AccelerationStructureHandle create_acceleration_structure(
        uint64_t acceleration_structure_size, VkAccelerationStructureTypeKHR type
    );

    void destroy_acceleration_structure(AccelerationStructureHandle handle);

    void destroy_framebuffer(Framebuffer&& framebuffer);

    /**
     * Get a sampler that matches the provided desc
     *
     * This method may create an actual sampler, or it may just return an existing one
     */
    VkSampler get_sampler(const VkSamplerCreateInfo& info);

    /**
     * Frees the resources in the zombie list for the given frame
     *
     * Should be called at the beginning of the frame by the backend
     *
     * @param frame_idx Index of the frame to delete resources for
     */
    void free_resources_for_frame(uint32_t frame_idx);

    void report_memory_usage() const;

    VmaAllocator get_vma() const;

private:
    RenderBackend& backend;

    VmaAllocator vma = VK_NULL_HANDLE;

    plf::colony<GpuTexture> textures;
    plf::colony<GpuBuffer> buffers;
    plf::colony<AccelerationStructure> acceleration_structures;

    eastl::unordered_map<std::string, VkRenderPass> cached_render_passes;

    eastl::array<eastl::vector<BufferHandle>, num_in_flight_frames> buffer_zombie_lists;
    eastl::array<eastl::vector<TextureHandle>, num_in_flight_frames> texture_zombie_lists;
    eastl::array<eastl::vector<AccelerationStructureHandle>, num_in_flight_frames> as_zombie_lists;
    eastl::array<eastl::vector<Framebuffer>, num_in_flight_frames> framebuffer_zombie_lists;

    struct SamplerCreateInfoHasher {
        std::size_t operator()(const VkSamplerCreateInfo& k) const {
            // Pretend that the create info is a byte array. That's basically all a struct is, no?
            // This hasher doesn't care about extensions. It's probably (not) fine
            return CityHash64(reinterpret_cast<const char*>(&k), sizeof(VkSamplerCreateInfo));
        }
    };

    // Cache from sampler create info hash to sampler
    // I do the hashing myself
    eastl::unordered_map<std::size_t, VkSampler> sampler_cache;
};

template <typename MappedType>
MappedType* ResourceAllocator::map_buffer(const BufferHandle buffer) {
    return static_cast<MappedType*>(map_buffer(buffer));
}
