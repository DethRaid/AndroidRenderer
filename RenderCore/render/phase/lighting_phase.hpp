#pragma once

#include <volk.h>

#include "render/backend/graphics_pipeline.hpp"
#include "render/backend/handles.hpp"

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

    void set_shadowmap(TextureHandle shadowmap_in);

    void render(CommandBuffer& commands, const SceneTransform& view, const std::unique_ptr<LightPropagationVolume>& lpv);

private:
    RenderScene* scene = nullptr;

    GBuffer gbuffer;

    GraphicsPipelineHandle emission_pipeline;

    void add_sun_lighting(CommandBuffer& commands, VkDescriptorSet gbuffers_descriptor_set, const SceneTransform& view) const;

    void add_raytraced_mesh_lighting(CommandBuffer& commands, VkDescriptorSet gbuffers_descriptor_set, BufferHandle view_buffer);

    void add_emissive_lighting(CommandBuffer& commands, VkDescriptorSet gbuffer_descriptor_set);

    TextureHandle shadowmap = TextureHandle::None;
};
