//
// Created by gold1 on 9/10/2022.
//

#ifndef SAHRENDERER_MESH_HPP
#define SAHRENDERER_MESH_HPP

#include <cstdint>

#include <vk_mem_alloc.h>

struct Mesh {
    VmaVirtualAllocation vertex_allocation;

    VmaVirtualAllocation index_allocation;

    VkDeviceSize first_index;

    uint32_t num_indices;

    VkDeviceSize first_vertex;

    uint32_t num_vertices;
};

#endif //SAHRENDERER_MESH_HPP
