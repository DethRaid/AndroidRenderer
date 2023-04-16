#pragma once

#include <cstdint>

#include <vk_mem_alloc.h>
#include <glm/vec3.hpp>

#include "render/backend/handles.hpp"

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

    float average_triangle_area = {};

    /**
     * \brief Buffer that stores the MeshPoints that make up the point cloud of this mesh's surface
     */
    BufferHandle point_cloud_buffer = BufferHandle::None;

    /**
     * \brief Buffer that stores a point cloud of this mesh, with a position + spherical harmonic of its normal. We
     * inject this into the LPV GV
     */
    BufferHandle sh_points_buffer = BufferHandle::None;

    uint32_t num_points = 0;
};
