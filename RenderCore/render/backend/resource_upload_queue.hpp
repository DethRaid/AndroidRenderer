#pragma once

#include <cstdint>
#include <vector>
#include <span>

#include "render/backend/handles.hpp"
#include "spdlog/logger.h"
#include "buffer.hpp"

class RenderBackend;
struct ktxTexture;

/**
 * Uploads some raw data to a texture
 */
struct TextureUploadJob {
    TextureHandle destination;
    uint32_t mip;
    std::vector<uint8_t> data;
};

/**
 * Uploads data from a KTX texture to a texture
 */
struct KtxUploadJob {
    TextureHandle destination;
    std::unique_ptr<ktxTexture, std::function<void(ktxTexture*)>> source;
};

struct BufferUploadJob {
    BufferHandle buffer;
    std::vector<uint8_t> data;
    uint32_t offset;
};

/**
 * Queues up resource uploads, then submits them
 *
 *
 */
class ResourceUploadQueue {
public:
    explicit ResourceUploadQueue(RenderBackend& backend_in);

    template<typename DataType>
    void upload_to_buffer(BufferHandle buffer, std::span<const DataType> data, uint32_t offset);

    void enqueue(KtxUploadJob&& job);

    /**
     * Enqueues a job to upload data to one mip of a texture
     *
     * The job is batched until the backend calls flush_pending_uploads
     *
     * @param job A job representing the upload to perform
     */
    void enqueue(TextureUploadJob&& job);

    void enqueue(BufferUploadJob&& job);

    /**
     * Flushes all pending uploads. Records them to a command list and submits it to the backend. Also issues barriers
     * to transition the uploaded-to mips to be shader readable
     */
    void flush_pending_uploads();

private:
    std::shared_ptr<spdlog::logger> logger;

    RenderBackend& backend;

    std::vector<TextureUploadJob> texture_uploads;

    std::vector<KtxUploadJob> ktx_uploads;

    std::vector<BufferUploadJob> buffer_uploads;

    void upload_ktx(VkCommandBuffer cmds, const KtxUploadJob& job, const Buffer& staging_buffer, size_t offset);
};

template <typename DataType>
void ResourceUploadQueue::upload_to_buffer(BufferHandle buffer, std::span<const DataType> data, uint32_t offset) {
    auto job = BufferUploadJob{
            .buffer = buffer,
            .data = {},
            .offset = offset,
    };
    job.data.resize(data.size() * sizeof(DataType));
    std::memcpy(job.data.data(), data.data(), data.size() * sizeof(DataType));

    enqueue(std::move(job));
}
