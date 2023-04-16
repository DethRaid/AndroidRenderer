#pragma once

#include <glm/glm.hpp>

#include "render/backend/handles.hpp"
#include "render/backend/command_buffer.hpp"
#include "render/scene_view_gpu.hpp"
#include "light_propagation_volume.hpp"

class ResourceAllocator;
class RenderBackend;

/**
 * A transform that can represent a camera viewing the scene
 */
class SceneTransform {
public:
    explicit SceneTransform(RenderBackend& backend_in);

    void set_render_resolution(glm::uvec2 render_resolution);
    
    void translate(glm::vec3 localspace_movement);

    /**
     * \brief Rotates the camera by the specified amount
     * \param delta_pitch Pitch, in radians
     * \param delta_yaw Yaw, in radians
     */
    void rotate(float delta_pitch, float delta_yaw);

    /**
     * Sets the position and forward direction of this scene view
     *
     * @param position_in Worldspace position
     */
    void set_position(const glm::vec3& position_in);

    void set_perspective_projection(float fov_in, float aspect_in, float near_value_in);

    BufferHandle get_buffer() const;

    void update_transforms(CommandBuffer commands);

    void set_aspect_ratio(float aspect_in);

    float get_near() const;

    float get_fov() const;

    float get_aspect_ratio() const;

    const SceneViewGpu& get_gpu_data() const;

    glm::vec3 get_position() const;

    glm::vec3 get_forward() const;

private:
    RenderBackend* backend = nullptr;

    float fov = {75.f};

    float aspect = 16.f / 9.f;

    float near_value = 0.05f;

    /**
     * Worldspace location of the camera
     */
    glm::vec3 position = glm::vec3{};

    /**
     * \brief Pitch of the view, in radians
     */
    float pitch = 0;

    /**
     * \brief Yaw of the view, in radians
     */
    float yaw = 0;

    glm::vec3 forward = {};

    SceneViewGpu gpu_data = {};

    BufferHandle buffer = BufferHandle::None;

    bool is_dirty = true;

    void refresh_view_matrices();
};



