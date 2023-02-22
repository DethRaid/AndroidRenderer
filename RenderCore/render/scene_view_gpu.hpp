#pragma once

#include <glm/glm.hpp>

struct SceneViewGpu {
    glm::mat4 view = {};
    glm::mat4 projection = {};

    glm::mat4 inverse_view = {};
    glm::mat4 inverse_projection = {};

    glm::vec4 render_resolution = {};
};
