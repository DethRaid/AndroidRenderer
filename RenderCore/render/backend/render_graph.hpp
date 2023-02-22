#pragma once

#include <vector>

#include "render/backend/render_pass.hpp"
#include "render/backend/compiled_render_pass.hpp"
#include "render_backend.hpp"

/**
 * Basic render graph
 *
 * Can automatically handle resource transitions, creating/destroying render passes, and much more
 *
 * Render passes are always executed in the order they're received. Intended usage is for you to make a new rneder graph
 * each frame, add passes to it, then submit it to the backend for execution
 */
class RenderGraph {
public:
    explicit RenderGraph(RenderBackend& backend_in);

    void add_pass(RenderPass&& pass);

    void execute();

private:
    /**
     * All the render passes that were submitted to this graph. Cleared every frame
     */
    std::vector<RenderPass> passes;

    /**
     * Compiled render passes, with barriers and VkRenderPasses already created. Persists across frames, may be
     * recreated if the input passes change
     */
    std::vector<CompiledRenderPass> compiled_passes;

    RenderBackend& backend;
};



