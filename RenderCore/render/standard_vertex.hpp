#ifndef SAHRENDERER_STANDARD_VERTEX_HPP
#define SAHRENDERER_STANDARD_VERTEX_HPP

#include <glm/glm.hpp>

struct StandardVertex {
    glm::vec3 position = {};
    glm::vec3 normal = glm::vec3{ 0.f, 0.f, 1.f };
    glm::vec3 tangent = {};
    glm::vec2 texcoord = {};
    uint32_t color = glm::packUnorm4x8(glm::vec4{ 1.f });
};

using VertexPosition = glm::vec3;

struct StandardVertexData {
    glm::vec3 normal = glm::vec3{ 0.f, 0.f, 1.f };
    glm::vec3 tangent = {};
    glm::vec2 texcoord = {};
    uint32_t color = glm::packUnorm4x8(glm::vec4{ 1.f });
};

#endif //SAHRENDERER_STANDARD_VERTEX_HPP
