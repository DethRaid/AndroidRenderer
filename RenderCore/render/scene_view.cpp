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

SceneView::SceneView(RenderBackend& backend_in) : backend{&backend_in} {
    auto& allocator = backend->get_global_allocator();
    buffer = allocator.create_buffer("Scene View Buffer", sizeof(SceneViewGpu),
                                     BufferUsage::UniformBuffer);
}

void SceneView::set_render_resolution(const glm::uvec2 render_resolution) {
    gpu_data.render_resolution.x = render_resolution.x;
    gpu_data.render_resolution.y = render_resolution.y;
    is_dirty = true;
}

void SceneView::set_position_and_direction(const glm::vec3& position, const glm::vec3& direction) {
    gpu_data.view = glm::lookAt(position, position + direction, glm::vec3{0.f, 1.f, 0.f});
    gpu_data.inverse_view = glm::inverse(gpu_data.view);
    is_dirty = true;
}

void SceneView::set_perspective_projection(const float fov_in, const float aspect_in, const float near_value_in) {
    fov = fov_in;
    aspect = aspect_in;
    near_value = near_value_in;

    gpu_data.projection = glm::tweakedInfinitePerspective(glm::radians(fov), aspect, near_value);
    // gpu_data.projection = infinitePerspectiveFovReverseZ_ZO(glm::radians(fov), aspect, 1.f, near_value);

#if defined(__ANDROID__)
    gpu_data.projection = glm::rotate(gpu_data.projection, glm::radians(270.f), glm::vec3{0, 0, 1});
#endif

    gpu_data.inverse_projection = glm::inverse(gpu_data.projection);

    is_dirty = true;
}

BufferHandle SceneView::get_buffer() const {
    return buffer;
}

void SceneView::update_transforms(CommandBuffer commands) {
    if (buffer != BufferHandle::None && is_dirty) {
        commands.update_buffer(buffer, gpu_data);

        commands.barrier(buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_WRITE_BIT,
                         VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT);

        is_dirty = false;
    }
}

void SceneView::set_aspect_ratio(const float aspect_in) {
    set_perspective_projection(fov, aspect_in, near_value);
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

const SceneViewGpu& SceneView::get_gpu_data() const {
    return gpu_data;
}

glm::vec3 SceneView::get_postion() const {
    return glm::vec3{gpu_data.view[3][0], gpu_data.view[3][1], gpu_data.view[3][2]} * -1.f;
}

glm::vec3 SceneView::get_forward() const {
    return glm::normalize(glm::vec3{gpu_data.inverse_view[3]});
}
