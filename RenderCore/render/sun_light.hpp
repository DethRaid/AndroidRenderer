#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "render/backend/handles.hpp"
#include "backend\graphics_pipeline.hpp"
#include "render/scene_view_gpu.hpp"
#include "shared/sun_light_constants.hpp"

class ResourceAllocator;

class CommandBuffer;
class SceneTransform;
class RenderBackend;

/**
 * Represents a sun light
 */
class SunLight {
public:
    explicit SunLight(const RenderBackend& backend);

    void update_shadow_cascades(const SceneTransform& view);

    void set_direction(const glm::vec3& direction);

    void set_color(glm::vec4 color);

    void update_buffer(CommandBuffer& commands);

    BufferHandle get_constant_buffer() const;

    GraphicsPipelineHandle& get_pipeline();

    glm::vec3 get_direction() const;

private:
    ResourceAllocator& allocator;

    bool sun_buffer_dirty = true;

    SunLightConstants constants = {};

    BufferHandle sun_buffer = BufferHandle::None;

    GraphicsPipelineHandle pipeline = {};
};
