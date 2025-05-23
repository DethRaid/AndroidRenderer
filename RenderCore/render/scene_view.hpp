#pragma once

#include <glm/glm.hpp>

#include "render/backend/handles.hpp"
#include "render/backend/command_buffer.hpp"
#include "backend/resource_upload_queue.hpp"
#include "shared/view_data.hpp"

class ResourceAllocator;
class RenderBackend;

/**
 * A class that can view a scene. Contains various camera and rendering parameters
 */
class SceneView {
public:
    explicit SceneView();

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

    void update_transforms(ResourceUploadQueue& upload_queue);

    void set_aspect_ratio(float aspect_in);

    void set_mip_bias(float mip_bias);

    float get_near() const;

    float get_fov() const;

    float get_aspect_ratio() const;

    const ViewDataGPU& get_gpu_data() const;

    glm::vec3 get_position() const;

    glm::vec3 get_forward() const;

    void set_jitter(glm::vec2 jitter_in);

    glm::vec2 get_jitter() const;

    const glm::mat4& get_projection() const;

    const glm::mat4& get_last_frame_projection() const;

    void increment_frame_count();

    uint32_t get_frame_count() const;

    const float4x4& get_view() const;

private:
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

    /**
     * The projection matrices encased within contain jitter
     */
    ViewDataGPU gpu_data = {};

    /**
     * Projection matrix with no jitter
     */
    glm::mat4 projection = {};

    /**
     * Previous projection matrix with no jitter
     */
    glm::mat4 last_frame_projection = {};

    BufferHandle buffer = {};

    bool is_dirty = true;

    glm::vec2 jitter = {};

    uint32_t frame_count = 0;

    void refresh_view_matrices();

    void refresh_projection_matrices();
};
