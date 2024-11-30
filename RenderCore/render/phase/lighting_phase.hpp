#pragma once

#include "render/backend/graphics_pipeline.hpp"
#include "render/backend/handles.hpp"

struct DescriptorSet;
class RenderGraph;
class CommandBuffer;
class RenderScene;
class RenderBackend;
class SceneTransform;
class LightPropagationVolume;

struct GBuffer {
    TextureHandle color = TextureHandle::None;
    TextureHandle normal = TextureHandle::None;
    TextureHandle data = TextureHandle::None;
    TextureHandle emission = TextureHandle::None;
    TextureHandle depth = TextureHandle::None;
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

    void render(RenderGraph& render_graph, const SceneTransform& view, TextureHandle lit_scene_texture, const LightPropagationVolume* lpv) const;

private:
    RenderScene* scene = nullptr;

    GBuffer gbuffer;

    GraphicsPipelineHandle emission_pipeline;

    void add_raytraced_mesh_lighting(CommandBuffer& commands, const DescriptorSet& gbuffers_descriptor_set, BufferHandle view_buffer) const;

    void add_emissive_lighting(CommandBuffer& commands, const DescriptorSet& gbuffer_descriptor_set) const;
};
