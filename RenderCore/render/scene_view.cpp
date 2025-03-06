#include "scene_view.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/reciprocal.hpp>

#include "render/backend/resource_allocator.hpp"
#include "render/backend/render_backend.hpp"
#include "shared/view_data.hpp"

glm::mat4 infinitePerspectiveFovReverseZ_ZO(
    const float fov, const float width, const float height, const float z_near
) {
    const auto ep = glm::epsilon<float>();
    const auto h = glm::cot(0.5f * fov);
    const auto w = h * height / width;
    auto result = glm::zero<glm::mat4>();
    result[0][0] = -w;
    result[1][1] = h;
    result[2][2] = 0.0f;
    result[2][3] = 1.0f;
    result[3][2] = z_near;
    return result;
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
    const auto right = glm::vec3{sin(yaw - 3.1415927 / 2.0), 0, cos(yaw - 3.14159 / 2.0)};
    const auto up = cross(right, forward);

    gpu_data.last_frame_view = gpu_data.view;
    gpu_data.view = glm::lookAt(position, position + forward, up);
    gpu_data.inverse_view = glm::inverse(gpu_data.view);

    is_dirty = true;
}

void SceneView::refresh_projection_matrices() {
    last_frame_projection = projection;

    projection = glm::tweakedInfinitePerspective(glm::radians(fov), aspect, near_value);
    // projection = infinitePerspectiveFovReverseZ_ZO(glm::radians(fov), aspect, 1.f, near_value);

#if defined(__ANDROID__)
    projection = glm::rotate(projection, glm::radians(270.f), glm::vec3{ 0, 0, 1 });
#endif

    gpu_data.last_frame_projection = gpu_data.projection;
    gpu_data.projection = projection;
    gpu_data.projection[0][2] += jitter.x;
    gpu_data.projection[1][2] += jitter.y;

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
