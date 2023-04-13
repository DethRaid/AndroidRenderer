#pragma once

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

    void fill_mip_chain(RenderGraph& graph, TextureHandle texture);

private:
    RenderBackend& backend;

    BufferHandle counter_buffer;

    ComputeShader shader;
};

