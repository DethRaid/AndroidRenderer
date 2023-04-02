#pragma once

#include <glm/vec3.hpp>

/**
 * Simple class to represent a box
 */
struct Box {
    glm::vec3 min = {};
    glm::vec3 max = {};

    bool overlaps(const Box& other) const;
};

