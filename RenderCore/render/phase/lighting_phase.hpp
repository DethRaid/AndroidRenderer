#pragma once

#include <optional>

#include "render/gbuffer.hpp"
#include "render/backend/graphics_pipeline.hpp"
#include "render/backend/handles.hpp"

class IGlobalIlluminator;
class RayTracedGlobalIllumination;
struct NoiseTexture;
class ProceduralSky;
struct DescriptorSet;
class RenderGraph;
class CommandBuffer;
class RenderScene;
class RenderBackend;
class SceneView;
class LightPropagationVolume;

/**
 * Computes the lighting form the gbuffers
 *
 * This pass adds in lighting from a variety of sources: the sun, the sky, indirect lighting, area
 * lights, etc
 */
class LightingPhase {
public:
    explicit LightingPhase();

    void set_scene(RenderScene& scene_in);

    void render(
        RenderGraph& render_graph,
        const SceneView& view,
        const GBuffer& gbuffer,
        TextureHandle lit_scene_texture,
        TextureHandle ao_texture,
        const IGlobalIlluminator* gi,
        std::optional<TextureHandle> vrsaa_shading_rate_image,
        const NoiseTexture& noise,
        TextureHandle noise_2d
    );

private:
    RenderScene* scene = nullptr;

    GraphicsPipelineHandle emission_pipeline;

    TextureHandle sky_occlusion_map = nullptr;

    void rasterize_sky_shadow(RenderGraph& render_graph, const SceneView& view);

    void add_raytraced_mesh_lighting(CommandBuffer& commands, BufferHandle view_buffer) const;

    void add_emissive_lighting(CommandBuffer& commands) const;
};
