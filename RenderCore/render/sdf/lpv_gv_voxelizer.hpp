#pragma once

#include <glm/vec3.hpp>

#include "render/mesh_drawer.hpp"
#include "render/mesh_handle.hpp"
#include "render/backend/compute_shader.hpp"
#include "render/backend/handles.hpp"
#include "..\backend\graphics_pipeline.hpp"
#include "render/backend/resource_allocator.hpp"

class MeshStorage;
class RenderBackend;
class RenderGraph;
class RenderScene;

/**
 * Rasterizes a scene to a 3D texture using some parameters
 *
 * First use case: Voxelize a geometry volume for the LPV. Seems like it's best to store the volume as SH
 *
 * Second use case: Unknown, but possibly generating a SDF of all or part of the scene. Possibly something else. It
 * feels like the vertex and geometry shaders will be similar, and the fragment shader will be completely different.
 * Might be worth trying to make a shader template system, which might one day help the material system use different
 * vertex functions and material functions
 */
class ThreeDeeRasterizer {
public:
    ThreeDeeRasterizer(RenderBackend& backend_in);

    ~ThreeDeeRasterizer();

    /**
     * Inits all intermediate resources needed to render num_triangles_ triangles at the given resolution
     */
    void init_resources(glm::uvec3 voxel_texture_resolution, uint32_t num_triangles);

    void deinit_resources(ResourceAllocator& allocator);

    void voxelize_mesh(RenderGraph& graph, MeshHandle mesh, const MeshStorage& meshes);

    /**
     * Extracts the rastered texture from the rasterizer, setting the internal texture to TextureHandle::None
     */
    TextureHandle extract_texture();

private:
    /**
     * Resolution we're currently rasterizing at
     */
    glm::uvec3 resolution = {};

    /**
     * Number of triangles we're rasterizing
     */
    uint32_t max_num_triangles = 0;

    TextureHandle voxel_texture = TextureHandle::None;

    BufferHandle volume_uniform_buffer = BufferHandle::None;

    BufferHandle transformed_triangle_cache = BufferHandle::None;

    BufferHandle triangle_sh_cache = BufferHandle::None;

    BufferHandle bins = BufferHandle::None;

    BufferHandle cell_bitmask_coarse = BufferHandle::None;

    BufferHandle cell_bitmask = BufferHandle::None;

    ComputeShader texture_clear_shader;

    ComputeShader transform_verts_shader;

    ComputeShader coarse_binning_shader;

    ComputeShader fine_binning_shader;

    ComputeShader rasterize_primitives_shader;

    ComputeShader normalize_gv_shader;

    RenderBackend* backend;
};
