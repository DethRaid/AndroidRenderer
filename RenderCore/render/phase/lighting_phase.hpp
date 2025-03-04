#pragma once

#include "render/backend/graphics_pipeline.hpp"
#include "render/backend/handles.hpp"

class ProceduralSky;
struct DescriptorSet;
class RenderGraph;
class CommandBuffer;
class RenderScene;
class RenderBackend;
class SceneView;
class LightPropagationVolume;

struct GBuffer {
    TextureHandle color = nullptr;
    TextureHandle normal = nullptr;
    TextureHandle data = nullptr;
    TextureHandle emission = nullptr;
    TextureHandle depth = nullptr;
};

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

    void set_gbuffer(const GBuffer& gbuffer_in);

    void render(
        RenderGraph& render_graph, 
        const SceneView& view, 
        TextureHandle lit_scene_texture, 
        TextureHandle ao_texture,
        const LightPropagationVolume* lpv, 
        const ProceduralSky& sky, 
        std::optional<TextureHandle> vrsaa_shading_rate_image
    ) const;

private:
    RenderScene* scene = nullptr;

    GBuffer gbuffer;

    GraphicsPipelineHandle emission_pipeline;

    void add_raytraced_mesh_lighting(
        CommandBuffer& commands, const DescriptorSet& gbuffers_descriptor_set, BufferHandle view_buffer
    ) const;

    void add_emissive_lighting(CommandBuffer& commands, const DescriptorSet& gbuffer_descriptor_set) const;
};
