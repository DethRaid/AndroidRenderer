#pragma once
#include <volk.h>
#include <absl/container/flat_hash_map.h>

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

    void set_resource_usage(TextureHandle texture, const TextureUsageToken& usage, bool skip_barrier = false);

    void set_resource_usage(BufferHandle buffer, VkPipelineStageFlags2 pipeline_stage, VkAccessFlags2 access);

    void issue_barriers(const CommandBuffer& commands);

    TextureUsageToken get_last_usage_token(TextureHandle texture_handle);

private:
    RenderBackend& backend;

    absl::flat_hash_map<BufferHandle, BufferUsageToken> initial_buffer_usages;

    absl::flat_hash_map<BufferHandle, BufferUsageToken> last_buffer_usages;

    TextureUsageMap initial_texture_usages;

    TextureUsageMap last_texture_usages;

    std::vector<VkBufferMemoryBarrier2> buffer_barriers;

    std::vector<VkImageMemoryBarrier2> image_barriers;
};

