#pragma once

#include <string>

#include <volk.h>
#include <vk_mem_alloc.h>
#include <ktxvulkan.h>

enum class TextureAllocationType {
    Vma,
    Ktx,
    Swapchain,
};

struct VmaTextureAllocation {
    VmaAllocation allocation;
    VmaAllocationInfo allocation_info;
};

struct KtxTextureAllocation {
    ktxVulkanTexture ktx_vk_tex;
};

struct GpuTexture {
    std::string name;

    VkImageCreateInfo create_info;

    VkImage image;
    VkImageView image_view;

    /**
     * Whether or not the image needs to be backed by real memory. Transient images may only be used as render targets
     * or input attachments. They only exist within a single renderpass
     */
    bool is_transient = false;

    /**
     * View to use when using this image as a render target. Probably the same as image_view for 2D images, may be a
     * 2D array view for 3D images
     */
    VkImageView attachment_view;

    TextureAllocationType type;

    /**
     * \brief Views that just look at one mip level of the image. Useful for single-pass-downsampling
     */
    eastl::vector<VkImageView> mip_views;

    union {
        VmaTextureAllocation vma;
        KtxTextureAllocation ktx;
    };

    bool operator==(const GpuTexture& other) const;

    glm::uvec2 get_resolution() const;
};

inline bool GpuTexture::operator==(const GpuTexture& other) const {
    return memcmp(this, &other, sizeof(GpuTexture)) == 0;
}

inline glm::uvec2 GpuTexture::get_resolution() const {
    return {create_info.extent.width, create_info.extent.height};
}
