#pragma once
#include <vector>
#include <volk.h>

#include "render/backend/buffer_usage_token.hpp"
#include "render/backend/texture_usage_token.hpp"
#include "render/backend/handles.hpp"

class CommandBuffer;
class RenderBackend;
/**
 * \brief Tracks resource access, and allows querying for resource barriers
 */
class ResourceAccessTracker{
public:
    explicit ResourceAccessTracker(RenderBackend& backend_in);

    void set_resource_usage(const TextureUsageToken& usage, bool skip_barrier = false);

    void set_resource_usage(const BufferUsageToken& usage);

    void issue_barriers(const CommandBuffer& commands);

    TextureUsageToken get_last_usage_token(TextureHandle texture_handle);

private:
    RenderBackend& backend;

    std::vector<BufferUsageToken> initial_buffer_usages;

    std::vector<BufferUsageToken> last_buffer_usages;

    std::vector<TextureUsageToken>  initial_texture_usages;

    std::vector<TextureUsageToken> last_texture_usages;

    std::vector<VkBufferMemoryBarrier2> buffer_barriers;

    std::vector<VkImageMemoryBarrier2> image_barriers;
};

