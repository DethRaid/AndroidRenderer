#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "render/backend/handles.hpp"
#include "render/backend/pipeline.hpp"
#include "render/scene_view_gpu.hpp"
#include "shared/sun_light_constants.hpp"

class ResourceAllocator;

class CommandBuffer;
class SceneView;
class RenderBackend;

/**
 * Represents a sun light
 */
class SunLight {
public:
    explicit SunLight(RenderBackend& backend);

    void update_shadow_cascades(SceneView& view);

    void set_direction(const glm::vec3& direction);

    void set_color(glm::vec4 color);

    void update_buffer(CommandBuffer& commands);

    BufferHandle get_constant_buffer() const;

    Pipeline& get_pipeline();

private:
    ResourceAllocator& allocator;

    bool sun_buffer_dirty = true;

    SunLightConstants constants = {};

    BufferHandle sun_buffer = BufferHandle::None;

    Pipeline pipeline = {};
};
