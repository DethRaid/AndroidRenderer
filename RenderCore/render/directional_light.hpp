#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "backend/acceleration_structure.hpp"
#include "render/backend/handles.hpp"
#include "backend/graphics_pipeline.hpp"
#include "shared/sun_light_constants.hpp"

class SceneDrawer;
class RenderGraph;
struct DescriptorSet;
class ResourceAllocator;

class CommandBuffer;
class SceneTransform;
class RenderBackend;

/**
 * Represents a directional light
 */
class DirectionalLight {
public:
    explicit DirectionalLight();

    void update_shadow_cascades(const SceneTransform& view);

    void set_direction(const glm::vec3& direction);

    void set_color(glm::vec4 color);

    void update_buffer(CommandBuffer& commands);

    BufferHandle get_constant_buffer() const;

    GraphicsPipelineHandle& get_pipeline();

    glm::vec3 get_direction() const;

    void render_shadows(RenderGraph& graph, const SceneDrawer& sun_shadow_drawer) const;

    void render(
        CommandBuffer& commands, const DescriptorSet& gbuffers_descriptor_set, const SceneTransform& view,
        AccelerationStructureHandle rtas
    ) const;

    TextureHandle get_shadowmap_handle() const;

private:
    bool sun_buffer_dirty = true;

    // Massive TODO: Replace this with the ViewDataGPU struct
    SunLightConstants constants = {};

    BufferHandle sun_buffer = {};

    GraphicsPipelineHandle pipeline = {};

    bool has_dummy_shadowmap = true;
    TextureHandle shadowmap_handle = TextureHandle::None;
};
