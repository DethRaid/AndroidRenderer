#ifndef SAHRENDERER_STANDARD_VERTEX_HPP
#define SAHRENDERER_STANDARD_VERTEX_HPP

#include <glm/glm.hpp>

// TODO: Compress down to 20 bytes
struct StandardVertex {
    glm::vec3 position = {};
    glm::vec3 normal = glm::vec3{ 0.f, 0.f, 1.f };
    glm::vec3 tangent = {};
    glm::vec2 texcoord = {};
    uint32_t color = glm::packUnorm4x8(glm::vec4{ 1.f });
};

#endif //SAHRENDERER_STANDARD_VERTEX_HPP
