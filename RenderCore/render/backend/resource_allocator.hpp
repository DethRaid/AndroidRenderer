#pragma once

#include <array>
#include <vector>
#include <unordered_map>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <volk.h>

#include "extern/cityhash/city_hash.hpp"
#include "render/backend/texture.hpp"
#include "render/backend/handles.hpp"
#include "core/object_pool.hpp"
#include "render/backend/buffer.hpp"
#include "render/backend/constants.hpp"
#include "framebuffer.hpp"

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
};

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
     * Indirect commands buffer. Written to by one shader, used as indirect dispath or draw arguments
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
     * @param format Format of the texture
     * @param resolution Resolution of the texture
     * @param num_mips Number of mipmaps in the image
     * @param usage How the texture will be used
     * @return A handle to the texture
     */
    TextureHandle create_texture(const std::string& name, VkFormat format, glm::uvec2 resolution, uint32_t num_mips,
                                 TextureUsage usage, uint32_t num_layers = 1);

    TextureHandle create_volume_texture(const std::string& name, VkFormat format, glm::uvec3 resolution, uint32_t num_mips, TextureUsage usage);

    TextureHandle emplace_texture(const std::string& name, Texture&& new_texture);

    const Texture& get_texture(TextureHandle handle) const;

    void destroy_texture(TextureHandle handle);

    BufferHandle create_buffer(const std::string& name, size_t size, BufferUsage usage);

    const Buffer& get_buffer(BufferHandle handle) const;

    void destroy_buffer(BufferHandle handle);

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

    VmaAllocator get_vma() const;

private:
    RenderBackend& backend;

    VmaAllocator vma = VK_NULL_HANDLE;

    ObjectPool<Texture> textures;
    ObjectPool<Buffer> buffers;

    std::array<std::vector<BufferHandle>, num_in_flight_frames> buffer_zombie_lists;
    std::array<std::vector<TextureHandle>, num_in_flight_frames> texture_zombie_lists;
    std::array<std::vector<Framebuffer>, num_in_flight_frames> framebuffer_zombie_lists;

    struct SamplerCreateInfoHasher
    {
        std::size_t operator()(const VkSamplerCreateInfo& k) const
        {
            // Pretend that the create info is a byte array. That's basically all a struct is, no?
            // This hasher doesn't care about extensions. It's probably (not) fine
            return CityHash64(reinterpret_cast<const char*>(&k), sizeof(VkSamplerCreateInfo));
        }
    };

    // Cache from sampler create info hash to sampler
    // I do the hashing myself
    std::unordered_map<std::size_t, VkSampler> sampler_cache;
};



