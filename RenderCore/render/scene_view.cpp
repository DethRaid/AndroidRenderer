#include "scene_view.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/reciprocal.hpp>
#include <numbers>

#include "render/backend/resource_allocator.hpp"
#include "render/backend/render_backend.hpp"
#include "shared/view_data.hpp"

// from https://github.com/Sunset-Flock/Timberdoodle/blob/14c5ac3a0abee46ecac178b09712d24719e6e0fa/src/camera.cpp#L165
glm::mat4 inf_depth_reverse_z_perspective(
    const float fov_rads, const float aspect, const float z_near
) {
    assert(abs(aspect - std::numeric_limits<float>::epsilon()) > 0.0f);

    const auto tan_half_fov_y = 1.0f / std::tan(fov_rads * 0.5f);

    glm::mat4x4 ret(0.0f);
    ret[0][0] = tan_half_fov_y / aspect;
    ret[1][1] = tan_half_fov_y;
    ret[2][2] = 0.0f;
    ret[2][3] = -1.0f;
    ret[3][2] = z_near;
    return ret;
}

SceneView::SceneView() {
    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();
    buffer = allocator.create_buffer("Scene View Buffer", sizeof(ViewDataGPU), BufferUsage::UniformBuffer);
}

void SceneView::set_render_resolution(const glm::uvec2 render_resolution) {
    gpu_data.render_resolution.x = static_cast<float>(render_resolution.x);
    gpu_data.render_resolution.y = static_cast<float>(render_resolution.y);
    is_dirty = true;
}

void SceneView::translate(const glm::vec3 localspace_movement) {
    const auto worldspace_movement = gpu_data.inverse_view * glm::vec4{localspace_movement, 0.f};
    position += glm::vec3{worldspace_movement};
}

void SceneView::rotate(const float delta_pitch, const float delta_yaw) {
    pitch += delta_pitch;
    yaw += delta_yaw;
}

void SceneView::set_position(const glm::vec3& position_in) {
    position = position_in;;
}

glm::vec4 normalize_plane(const glm::vec4 p) {
    return p / length(glm::vec3{p});
}

void SceneView::set_perspective_projection(const float fov_in, const float aspect_in, const float near_value_in) {
    fov = fov_in;
    aspect = aspect_in;
    near_value = near_value_in;

    is_dirty = true;
}

BufferHandle SceneView::get_buffer() const {
    return buffer;
}

void SceneView::update_transforms(ResourceUploadQueue& upload_queue) {
    refresh_view_matrices();
    refresh_projection_matrices();

    if(buffer && is_dirty) {
        upload_queue.upload_to_buffer(buffer, gpu_data);

        is_dirty = false;
    }
}

void SceneView::set_aspect_ratio(const float aspect_in) {
    set_perspective_projection(fov, aspect_in, near_value);
}

void SceneView::set_mip_bias(const float mip_bias) {
    gpu_data.material_texture_mip_bias = mip_bias;
    is_dirty = true;
}

float SceneView::get_near() const {
    return near_value;
}

float SceneView::get_fov() const {
    return fov;
}

float SceneView::get_aspect_ratio() const {
    return aspect;
}

const ViewDataGPU& SceneView::get_gpu_data() const {
    return gpu_data;
}

glm::vec3 SceneView::get_position() const {
    return position;
}

glm::vec3 SceneView::get_forward() const {
    return forward;
}

void SceneView::set_jitter(const glm::vec2 jitter_in) {
    jitter = jitter_in;

    gpu_data.previous_jitter = gpu_data.jitter;
    gpu_data.jitter = jitter;

    is_dirty = true;
}

glm::vec2 SceneView::get_jitter() const {
    return jitter;
}

const glm::mat4& SceneView::get_projection() const { return projection; }

const glm::mat4& SceneView::get_last_frame_projection() const { return last_frame_projection; }

void SceneView::refresh_view_matrices() {
    forward = glm::vec3{cos(pitch) * sin(yaw), sin(pitch), cos(pitch) * cos(yaw)};
    const auto right = glm::vec3{sin(yaw - std::numbers::pi_v<float> / 2.0), 0, cos(yaw - std::numbers::pi_v<float> / 2.0)};
    const auto up = cross(right, forward);

    gpu_data.last_frame_view = gpu_data.view;
    gpu_data.view = glm::lookAt(position, position + forward, up);
    gpu_data.inverse_view = glm::inverse(gpu_data.view);

    is_dirty = true;
}

void SceneView::refresh_projection_matrices() {
    last_frame_projection = projection;

    // projection = glm::tweakedInfinitePerspective(glm::radians(fov), aspect, near_value);
    projection = inf_depth_reverse_z_perspective(glm::radians(fov), aspect, near_value);

#if defined(__ANDROID__)
    projection = glm::rotate(projection, glm::radians(270.f), glm::vec3{ 0, 0, 1 });
#endif

    gpu_data.last_frame_projection = gpu_data.projection;
    gpu_data.projection = projection;

    gpu_data.projection[2][0] += jitter.x * 2.f / gpu_data.render_resolution.x;
    gpu_data.projection[2][1] += jitter.y * 2.f / gpu_data.render_resolution.y;

    gpu_data.inverse_projection = glm::inverse(gpu_data.projection);

    glm::mat4 projection_t = glm::transpose(gpu_data.projection);

    // Why do we do this? See https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf
    glm::vec4 frustum_x = normalize_plane(projection_t[3] + projection_t[0]); // x + w < 0
    glm::vec4 frustum_y = normalize_plane(projection_t[3] + projection_t[1]); // y + w < 0

    if(fov > 0) {
        gpu_data.frustum[0] = frustum_x.x;
        gpu_data.frustum[1] = frustum_x.z;
        gpu_data.frustum[2] = frustum_y.y;
        gpu_data.frustum[3] = frustum_y.z;
    } else {
        gpu_data.frustum[0] = frustum_x.x;
        gpu_data.frustum[1] = frustum_x.w;
        gpu_data.frustum[2] = frustum_y.y;
        gpu_data.frustum[3] = frustum_y.w;
    }

    is_dirty = true;
}
