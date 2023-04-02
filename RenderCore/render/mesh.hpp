#pragma once

#include <cstdint>

#include <vk_mem_alloc.h>
#include <glm/vec3.hpp>

struct Mesh {
    VmaVirtualAllocation vertex_allocation = {};

    VmaVirtualAllocation index_allocation = {};

    VkDeviceSize first_index = 0;

    uint32_t num_indices = 0;

    VkDeviceSize first_vertex = 0;

    uint32_t num_vertices = 0;

    /**
     * Worldspace bounds of the mesh
     */
    glm::vec3 bounds = {};
};
