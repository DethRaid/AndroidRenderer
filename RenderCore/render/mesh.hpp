#pragma once

#include <cstdint>

#include <vk_mem_alloc.h>

#include "core/box.hpp"
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
    Box bounds = {};

    float average_triangle_area = {};

    /**
     * \brief Buffer that stores the MeshPoints that make up the point cloud of this mesh's surface
     */
    BufferHandle point_cloud_buffer = {};

    /**
     * \brief Buffer that stores a point cloud of this mesh, with a position + spherical harmonic of its normal. We
     * inject this into the LPV GV
     */
    BufferHandle sh_points_buffer = {};

    uint32_t num_points = 0;
};
