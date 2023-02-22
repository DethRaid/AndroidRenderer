#pragma once

#include "render/backend/handles.hpp"
#include "render/backend/pipeline.hpp"

class CommandBuffer;
class SceneRenderer;
class SunLight;

struct RsmTargets {
    TextureHandle rsm_flux;
    TextureHandle rsm_normal;
    TextureHandle rsm_depth;
};

/**
 * Extracts VPLs from the RSM targets
 */
class RsmVplPhase {
public:
    RsmVplPhase(SceneRenderer& renderer_in);

    void set_rsm(const RsmTargets& rsm_in);

    void setup_buffers(CommandBuffer& commands);

    void render(CommandBuffer& commands, const SunLight& light);

    const std::vector<BufferHandle>& get_vpl_lists() const;

private:
    SceneRenderer& renderer;

    RsmTargets rsm;

    Pipeline vpl_pipeline;

    std::vector<BufferHandle> count_buffers;

    std::vector<BufferHandle> vpl_buffers;
};



