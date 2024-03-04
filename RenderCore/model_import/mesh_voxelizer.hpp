#pragma once

#include "render/scene_primitive.hpp"
#include "render/backend/compute_shader.hpp"
#include "render/backend/graphics_pipeline.hpp"
#include "render/backend/handles.hpp"

class MeshStorage;
class RenderGraph;

struct VoxelTextures {
    glm::uvec3 num_voxels;
    TextureHandle color_texture;
    TextureHandle normals_texture;
};

/**
 * \brief Voxelizes a mesh, using all the things from https://developer.nvidia.com/content/basics-gpu-voxelization
 *
 * This can only run on desktop due to conservative rasterization
 */
class MeshVoxelizer {
public:
    /**
     * \brief How exactly to perform voxelization
     */
    enum class Mode {
        /**
         * \brief Rasterize the mesh's color to a 3D texture. The cells with geometry in them will have a non-zero
         * color and non-zero alpha, the cells without geometry will have a pure black color and an alpha of 0
         */
        ColorOnly,
    };

    explicit MeshVoxelizer(RenderBackend& backend_in);

    /**
     * \brief Voxelizes a primitive
     *
     * A primitive is a mesh + material that's been placed in the scene. We don't want to generate duplicate voxel
     * textures if you place the same mesh + material in the scene multiple times - but my structs don't make that
     * easy. The VoxelCache should check if there's a voxel texture for hte mesh + material combo before calling this
     * method
     *
     * \param graph RenderGraph to use to execute the voxelization
     * \param primitive The primitive to voxelize
     * \param mesh_storage The storage for mesh data, like vertex and index buffers
     * \param primitive_buffer The buffer that stores all the primitive infos on the GPU
     * \param mode How to voxelize the mesh
     * \param voxel_size Size of one edge of a voxel, in modelspace
     */
    VoxelTextures voxelize_primitive(
        RenderGraph& graph, MeshPrimitiveHandle primitive, const MeshStorage& mesh_storage,
        BufferHandle primitive_buffer, float voxel_size = 0.25, Mode mode = Mode::ColorOnly
    ) const;

private:
    RenderBackend* backend;

    GraphicsPipelineHandle voxelization_pipeline;

    ComputePipelineHandle compute_voxelization_pipeline;

    VoxelTextures voxelize_with_raster(
        RenderGraph& graph, MeshPrimitiveHandle primitive, const MeshStorage& mesh_storage,
        BufferHandle primitive_buffer,
        float voxel_size
    ) const;

    VoxelTextures voxelize_with_compute(
        RenderGraph& graph, MeshPrimitiveHandle primitive, const MeshStorage& mesh_storage,
        BufferHandle primitive_buffer, float voxel_size
    ) const;
};
