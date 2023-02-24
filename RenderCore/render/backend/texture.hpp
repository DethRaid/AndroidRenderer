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

struct Texture {
    std::string name;

    VkImageCreateInfo create_info;

    VkImage image;
    VkImageView image_view;

    TextureAllocationType type;

    union {
        VmaTextureAllocation vma;
        KtxTextureAllocation ktx;
    };
};
