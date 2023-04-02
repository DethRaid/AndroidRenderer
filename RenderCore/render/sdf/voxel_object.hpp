#pragma once

#include <glm/vec3.hpp>

#include "render/backend/handles.hpp"

/**
 * Represents an object made out of voxels
 */
struct VoxelObject {
    /**
     * Size of the voxel volume, in world units
     */
    glm::vec3 worldspace_size;

    /**
     * Texture that stores the SH representation of this object
     */
    TextureHandle sh_texture;
};
