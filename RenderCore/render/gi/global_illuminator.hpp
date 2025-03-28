#pragma once
#include <EASTL/vector.h>

#include "render/backend/buffer_usage_token.hpp"
#include "render/backend/handles.hpp"
#include "render/backend/texture_usage_token.hpp"

class CommandBuffer;
struct GBuffer;
class RenderGraph;
class SceneView;
class RenderScene;

/**
 * Interface for classes that can compute global illumination. Contains hooks to do some work before rendering the
 * scene, after rendering the scene, and for applying the lighting contribution to the lit scene render target
 */
class IGlobalIlluminator {
public:
    virtual ~IGlobalIlluminator() = default;

    virtual void pre_render(
        RenderGraph& graph, const SceneView& view, const RenderScene& scene, TextureHandle noise_tex
    ) = 0;

    virtual void post_render(
        RenderGraph& graph, const SceneView& view, const RenderScene& scene, const GBuffer& gbuffer,
        TextureHandle noise_tex
    ) = 0;

    virtual void get_lighting_resource_usages(
        TextureUsageList& textures, BufferUsageList& buffers
    ) const = 0;

    virtual void render_to_lit_scene(
        CommandBuffer& commands, BufferHandle view_buffer, TextureHandle ao_tex, TextureHandle noise_tex
    ) const = 0;

    /**
     * Draws any debug overlays you may want 
     */
    virtual void draw_debug_overlays(
        RenderGraph& graph, const SceneView& view, const GBuffer& gbuffer, TextureHandle lit_scene_texture
    ) = 0;
};
