//
// Created by gold1 on 8/25/2022.
//

#ifndef SAHRENDERER_STANDARD_VERTEX_HPP
#define SAHRENDERER_STANDARD_VERTEX_HPP

#include <glm/glm.hpp>

struct StandardVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec2 texcoord;
    uint32_t color;
};

using VertexPosition = glm::vec3;

struct StandardVertexData {
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec2 texcoord;
    uint32_t color;
};

#endif //SAHRENDERER_STANDARD_VERTEX_HPP
