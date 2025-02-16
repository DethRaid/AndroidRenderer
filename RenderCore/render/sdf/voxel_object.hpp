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
     * \brief 3D texture storing the average color in each voxel. Alpha is the alpha of this voxel
     *
     * RGBA 8-bit UNORM, but the data is stored in sRGB. You MUST convert to/from sRGB yourself
     */
    TextureHandle voxels_color;

    /**
     * \brief 3D texture storing the average normal in the center of each voxel. Alpha is unused
     *
     * R16G16B16A16 SNORM. Might eventually use R32 and pack R11G11B10 in myself
     */
    TextureHandle voxels_normals;
};
