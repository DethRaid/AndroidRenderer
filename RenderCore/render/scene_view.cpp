#include "scene_view.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/reciprocal.hpp>

#include "render/backend/resource_allocator.hpp"
#include "render/backend/render_backend.hpp"

glm::mat4 infinitePerspectiveFovReverseZ_ZO(const float fov, const float width, const float height, const float z_near) {
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

SceneTransform::SceneTransform(RenderBackend& backend_in) : backend{ &backend_in } {
    auto& allocator = backend->get_global_allocator();
    buffer = allocator.create_buffer("Scene View Buffer", sizeof(SceneViewGpu),
        BufferUsage::UniformBuffer);
}

void SceneTransform::set_render_resolution(const glm::uvec2 render_resolution) {
    gpu_data.render_resolution.x = static_cast<float>(render_resolution.x);
    gpu_data.render_resolution.y = static_cast<float>(render_resolution.y);
    is_dirty = true;
}

void SceneTransform::translate(const glm::vec3 localspace_movement) {
    const auto worldspace_movement = gpu_data.inverse_view * glm::vec4{ localspace_movement, 0.f };
    position += glm::vec3{ worldspace_movement };

    refresh_view_matrices();
}

void SceneTransform::rotate(const float delta_pitch, const float delta_yaw) {
    pitch += delta_pitch;
    yaw += delta_yaw;

    refresh_view_matrices();
}

void SceneTransform::set_position(const glm::vec3& position_in) {
    position = position_in;

    refresh_view_matrices();
}

void SceneTransform::set_perspective_projection(const float fov_in, const float aspect_in, const float near_value_in) {
    fov = fov_in;
    aspect = aspect_in;
    near_value = near_value_in;

    gpu_data.projection = glm::tweakedInfinitePerspective(glm::radians(fov), aspect, near_value);
    // gpu_data.projection = infinitePerspectiveFovReverseZ_ZO(glm::radians(fov), aspect, 1.f, near_value);

#if defined(__ANDROID__)
    gpu_data.projection = glm::rotate(gpu_data.projection, glm::radians(270.f), glm::vec3{ 0, 0, 1 });
#endif

    gpu_data.inverse_projection = glm::inverse(gpu_data.projection);

    is_dirty = true;
}

BufferHandle SceneTransform::get_buffer() const {
    return buffer;
}

void SceneTransform::update_transforms(CommandBuffer commands) {
    if (buffer != BufferHandle::None && is_dirty) {
        commands.update_buffer(buffer, gpu_data);

        commands.barrier(buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_WRITE_BIT,
            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT);

        is_dirty = false;
    }
}

void SceneTransform::set_aspect_ratio(const float aspect_in) {
    set_perspective_projection(fov, aspect_in, near_value);
}

float SceneTransform::get_near() const {
    return near_value;
}

float SceneTransform::get_fov() const {
    return fov;
}

float SceneTransform::get_aspect_ratio() const {
    return aspect;
}

const SceneViewGpu& SceneTransform::get_gpu_data() const {
    return gpu_data;
}

glm::vec3 SceneTransform::get_position() const {
    return position;
}

glm::vec3 SceneTransform::get_forward() const {
    return forward;
}

void SceneTransform::refresh_view_matrices() {
    forward = glm::vec3{cos(pitch) * sin(yaw), sin(pitch), cos(pitch) * cos(yaw)};
    const auto right = glm::vec3{ sin(yaw - 3.1415927 / 2.0), 0, cos(yaw - 3.14159 / 2.0) };
    const auto up = cross(right, forward);

    gpu_data.view = glm::lookAt(position, position + forward, up);
    gpu_data.inverse_view = glm::inverse(gpu_data.view);
    is_dirty = true;
}
