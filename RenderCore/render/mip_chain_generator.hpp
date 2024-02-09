#pragma once

#include <unordered_map>

#include "render/backend/compute_shader.hpp"
#include "render/backend/handles.hpp"

class RenderGraph;
class RenderBackend;
/**
 * \brief Beautiful class that generates a mip chain for an image
 */
class MipChainGenerator {
public:
    explicit MipChainGenerator(RenderBackend& backend_in);

    /**
     * \brief Builds a mip chain in the dest texture
     *
     * This method takes the image data from mip 0 of the src texture, and uses it to build a mip chain. The mip chain
     * is placed in the dest texture. The dest texture's mip 0 should be half the resolution of the src texture's mip 0
     *
     * This method handles the case of building a mip chain for a depth buffer
     *
     * \param graph Render graph to use to build the mip chain
     * \param src_texture Source of the image data
     * \param dest_texture Destination for the mip chain
     */
    void fill_mip_chain(RenderGraph& graph, TextureHandle src_texture, TextureHandle dest_texture);

private:
    RenderBackend& backend;

    BufferHandle counter_buffer;

    /**
     * \brief Map from texture format to the compute shader for that format
     *
     * Supports a few formats:
     * - VK_FORMAT_R16_SFLOAT: Bilinear filter 
     * - VK_FORMAT_R16G16B16A16_SFLOAT: Bilinear filter
     * - VK_FORMAT_B10G11R11_UFLOAT_PACK32: Bilinear filter
     * - VK_FORMAT_R32_SFLOAT: Max filter
     *
     * R32 is weird. It's the destination format when building a HiZ buffer, so we use a special shader. This is very weird and we do not like it
     */
    std::unordered_map<VkFormat, ComputeShader> shaders;

    VkSampler sampler;
};
