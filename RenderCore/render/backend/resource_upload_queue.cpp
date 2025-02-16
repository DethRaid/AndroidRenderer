#include "resource_upload_queue.hpp"

#include <ktx.h>

#include "render/backend/render_backend.hpp"
#include "utils.hpp"
#include "core/system_interface.hpp"

ResourceUploadQueue::ResourceUploadQueue(RenderBackend& backend_in) : backend{backend_in} {
    logger = SystemInterface::get().get_logger("ResourceUploadQueue");
}

void ResourceUploadQueue::enqueue(KtxUploadJob&& job) {
    ktx_uploads.emplace_back(std::move(job));
}

void ResourceUploadQueue::enqueue(TextureUploadJob&& job) {
    texture_uploads.emplace_back(std::move(job));
}

void ResourceUploadQueue::enqueue(BufferUploadJob&& job) {
    buffer_uploads.emplace_back(std::move(job));
}

void ResourceUploadQueue::flush_pending_uploads() {
    size_t total_size = 0;

    auto& allocator = backend.get_global_allocator();

    auto before_image_barriers = std::vector<VkImageMemoryBarrier2>{};
    auto after_image_barriers = std::vector<VkImageMemoryBarrier2>{};
    before_image_barriers.reserve(texture_uploads.size());
    after_image_barriers.reserve(texture_uploads.size() + ktx_uploads.size());

    // Look through all the upload jobs to see how big of a staging buffer we need, and to collect all the barriers

    for(const auto& job : ktx_uploads) {
        const auto job_size = ktxTexture_GetDataSizeUncompressed(job.source.get());
        total_size += job_size;

        before_image_barriers.emplace_back(
            VkImageMemoryBarrier2{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask = VK_ACCESS_2_NONE,
                .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .image = job.destination->image,
                .subresourceRange = {
                    .aspectMask = static_cast<VkImageAspectFlags>(
                        is_depth_format(job.destination->create_info.format)
                        ? VK_IMAGE_ASPECT_DEPTH_BIT
                        : VK_IMAGE_ASPECT_COLOR_BIT),
                    .baseMipLevel = 0,
                    .levelCount = job.source->numLevels,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                },
            }
        );
        after_image_barriers.emplace_back(
            VkImageMemoryBarrier2{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .image = job.destination->image,
                .subresourceRange = {
                    .aspectMask = static_cast<VkImageAspectFlags>(
                        is_depth_format(job.destination->create_info.format)
                        ? VK_IMAGE_ASPECT_DEPTH_BIT
                        : VK_IMAGE_ASPECT_COLOR_BIT),
                    .baseMipLevel = 0,
                    .levelCount = job.source->numLevels,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                },
            }
        );
    }

    for(const auto& job : texture_uploads) {
        total_size += job.data.size();

        const auto aspect = static_cast<VkImageAspectFlags>(is_depth_format(job.destination->create_info.format)
            ? VK_IMAGE_ASPECT_DEPTH_BIT
            : VK_IMAGE_ASPECT_COLOR_BIT);

        before_image_barriers.emplace_back(
            VkImageMemoryBarrier2{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask = VK_ACCESS_2_NONE,
                .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .image = job.destination->image,
                .subresourceRange = {
                    .aspectMask = aspect,
                    .baseMipLevel = job.mip,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                },
            }
        );
        after_image_barriers.emplace_back(
            VkImageMemoryBarrier2{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .image = job.destination->image,
                .subresourceRange = {
                    .aspectMask = aspect,
                    .baseMipLevel = job.mip,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                },
            }
        );
    }

    auto before_buffer_barriers = std::vector<VkBufferMemoryBarrier2>{};
    auto after_buffer_barriers = std::vector<VkBufferMemoryBarrier2>{};
    before_buffer_barriers.reserve(buffer_uploads.size());
    after_buffer_barriers.reserve(buffer_uploads.size());

    for(const auto& job : buffer_uploads) {
        total_size += job.data.size();

        before_buffer_barriers.emplace_back(
            VkBufferMemoryBarrier2{
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .buffer = job.buffer->buffer,
                .offset = job.dest_offset,
                .size = job.data.size(),
            }
        );
        after_buffer_barriers.emplace_back(
            VkBufferMemoryBarrier2{
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
                .buffer = job.buffer->buffer,
                .offset = job.dest_offset,
                .size = job.data.size(),
            }
        );
    }

    if(total_size == 0) {
        // Nothing to upload, we can sleep easy
        return;
    }

    // logger->info("Creating a staging buffer of size {}", total_size);

    auto staging_buffer = allocator.create_buffer(
        "Upload staging buffer",
        total_size,
        BufferUsage::StagingBuffer
    );

    auto* data_write_ptr = static_cast<uint8_t*>(staging_buffer->allocation_info.pMappedData);

    // Copy the texture data to the staging buffer, and record the upload commands

    auto cmds = backend.create_transfer_command_buffer("Transfer command buffer");

    constexpr auto begin_info = VkCommandBufferBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    vkBeginCommandBuffer(cmds, &begin_info);

    const auto before_dependency_info = VkDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = static_cast<uint32_t>(before_buffer_barriers.size()),
        .pBufferMemoryBarriers = before_buffer_barriers.data(),
        .imageMemoryBarrierCount = static_cast<uint32_t>(before_image_barriers.size()),
        .pImageMemoryBarriers = before_image_barriers.data()
    };
    vkCmdPipelineBarrier2(cmds, &before_dependency_info);

    size_t cur_offset = 0;

    for(const auto& job : ktx_uploads) {
        upload_ktx(cmds, job, *staging_buffer, cur_offset);

        cur_offset += ktxTexture_GetDataSizeUncompressed(job.source.get());
    }

    for(const auto& job : texture_uploads) {
        // Copy data
        std::memcpy(data_write_ptr + cur_offset, job.data.data(), job.data.size());

        const auto region = VkBufferImageCopy{
            .bufferOffset = cur_offset,
            .imageSubresource = {
                .aspectMask = static_cast<VkImageAspectFlags>(is_depth_format(job.destination->create_info.format)
                    ? VK_IMAGE_ASPECT_DEPTH_BIT
                    : VK_IMAGE_ASPECT_COLOR_BIT),
                .mipLevel = job.mip,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .imageOffset = {},
            .imageExtent = job.destination->create_info.extent,
        };
        vkCmdCopyBufferToImage(
            cmds,
            staging_buffer->buffer,
            job.destination->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );

        cur_offset += job.data.size();
    }

    for(const auto& job : buffer_uploads) {
        // Copy data
        std::memcpy(data_write_ptr + cur_offset, job.data.data(), job.data.size());

        const auto region = VkBufferCopy{
            .srcOffset = cur_offset,
            .dstOffset = job.dest_offset,
            .size = job.data.size(),
        };
        vkCmdCopyBuffer(cmds, staging_buffer->buffer, job.buffer->buffer, 1, &region);

        cur_offset += job.data.size();
    }

    const auto after_dependency_info = VkDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = static_cast<uint32_t>(after_buffer_barriers.size()),
        .pBufferMemoryBarriers = after_buffer_barriers.data(),
        .imageMemoryBarrierCount = static_cast<uint32_t>(after_image_barriers.size()),
        .pImageMemoryBarriers = after_image_barriers.data()
    };

    vkCmdPipelineBarrier2(cmds, &after_dependency_info);

    vkEndCommandBuffer(cmds);

    backend.submit_transfer_command_buffer(cmds);

    ktx_uploads.clear();
    texture_uploads.clear();
    buffer_uploads.clear();
}

void ResourceUploadQueue::upload_ktx(
    const VkCommandBuffer cmds,
    const KtxUploadJob& job,
    const GpuBuffer& staging_buffer,
    const size_t offset
) const {
    const auto num_copy_regions = job.source->numLevels;
    auto copy_regions = std::vector<VkBufferImageCopy>(num_copy_regions);

    const auto data_size = ktxTexture_GetDataSizeUncompressed(job.source.get());
    auto* data_dest = static_cast<uint8_t*>(staging_buffer.allocation_info.pMappedData) + offset;

    if(job.source->pData) {
        // image is loaded, copy it to the buffer
        std::memcpy(data_dest, job.source->pData, job.source->dataSize);
    } else {
        // Load image to the buffer
        ktxTexture_LoadImageData(job.source.get(), data_dest, data_size);
    }

    // Set up the copy regions
    struct UserCbdataOptimal {
        VkBufferImageCopy* region; // Specify destination region in final image.
        VkDeviceSize offset; // Offset of current level in staging buffer
        ktx_uint32_t numFaces;
        ktx_uint32_t numLayers;
        // The following are used only by optimalTilingPadCallback
        ktx_uint8_t* dest; // Pointer to mapped staging buffer.
        ktx_uint32_t elementSize;
        ktx_uint32_t numDimensions;
    };

    auto copy_buffer_data = UserCbdataOptimal{
        .region = copy_regions.data(),
        .offset = 0,
        .numFaces = job.source->numFaces,
        .numLayers = job.source->numLayers,
        .dest = data_dest,
        .elementSize = ktxTexture_GetElementSize(job.source.get()),
        .numDimensions = job.source->numDimensions
    };

    auto iterate_func = +[](
        const int miplevel, const int face,
        const int width, const int height, const int depth,
        const ktx_uint64_t faceLodSize,
        void* pixels, void* userdata
    ) -> KTX_error_code {
            auto* ud = static_cast<UserCbdataOptimal*>(userdata);

            // Set up copy to destination region in final image
            ud->region->bufferOffset = ud->offset;
            ud->offset += faceLodSize;
            // These 2 are expressed in texels.
            ud->region->bufferRowLength = 0;
            ud->region->bufferImageHeight = 0;
            ud->region->imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ud->region->imageSubresource.mipLevel = miplevel;
            ud->region->imageSubresource.baseArrayLayer = face;
            ud->region->imageSubresource.layerCount = ud->numLayers * ud->numFaces;
            ud->region->imageOffset.x = 0;
            ud->region->imageOffset.y = 0;
            ud->region->imageOffset.z = 0;
            ud->region->imageExtent.width = width;
            ud->region->imageExtent.height = height;
            ud->region->imageExtent.depth = depth;

            ud->region += 1;

            return KTX_SUCCESS;
        };

    ktxTexture_IterateLevels(job.source.get(), iterate_func, &copy_buffer_data);

    vkCmdCopyBufferToImage(
        cmds,
        staging_buffer.buffer,
        job.destination->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        static_cast<uint32_t>(copy_regions.size()),
        copy_regions.data()
    );
}
