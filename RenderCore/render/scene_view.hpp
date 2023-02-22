#pragma once

#include <glm/glm.hpp>

#include "render/backend/handles.hpp"
#include "render/backend/command_buffer.hpp"
#include "render/scene_view_gpu.hpp"
#include "light_propagation_volume.hpp"

class ResourceAllocator;
class RenderBackend;

/**
 * Stinky view of the scene
 *
 * Might represent the player or a shadow camera
 */
class SceneView {
public:
    explicit SceneView(RenderBackend& backend_in);

    void set_render_resolution(const glm::uvec2 render_resolution);

    /**
     * Sets the position and forward direction of this scene view
     *
     * @param position Worldspace position
     * @param direction Worldspace forward direction
     */
    void set_position_and_direction(const glm::vec3& position, const glm::vec3& direction);

    void set_perspective_projection(float fov_in, float aspect_in, float near_value_in);

    BufferHandle get_buffer() const;

    void update_transforms(CommandBuffer commands);

    void set_aspect_ratio(float aspect_in);

    float get_near() const;

    float get_fov() const;

    float get_aspect_ratio() const;

    const SceneViewGpu& get_gpu_data() const;

    glm::vec3 get_postion() const;

    glm::vec3 get_forward() const;

private:
    RenderBackend* backend = nullptr;

    float fov = {75.f};

    float aspect = 16.f / 9.f;

    float near_value = 0.05f;

    SceneViewGpu gpu_data = {};

    BufferHandle buffer = BufferHandle::None;

    bool is_dirty = true;
};



