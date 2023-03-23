#pragma once

#include <glm/vec3.hpp>

#include "render/mesh_drawer.hpp"
#include "render/backend/compute_shader.hpp"
#include "render/backend/handles.hpp"
#include "render/backend/pipeline.hpp"
#include "render/backend/resource_allocator.hpp"

class MeshStorage;
class RenderBackend;
class RenderGraph;
class RenderScene;

/**
 * Voxelizes a scene to a 3D texture using some parameters
 *
 * First use case: Voxelize a geometry volume for the LPV. Seems like it's best to store the volume as SH
 *
 * Second use case: Unknown, but possibly generating a SDF of all or part of the scene. Possibly something else. It
 * feels like the vertex and geometry shaders will be similar, and the fragment shader will be completely different.
 * Might be worth trying to make a shader template system, which might one day help the material system use different
 * vertex functions and material functions
 */
class LpvGvVoxelizer {
public:
    explicit LpvGvVoxelizer() = default;

    ~LpvGvVoxelizer();

    void init_resources(RenderBackend& backend_in, uint32_t voxel_texture_resolution);

    void deinit_resources(ResourceAllocator& allocator);

    void set_scene(RenderScene& scene_in, MeshStorage& meshes_in);

    void voxelize_scene(RenderGraph& graph, const glm::vec3& voxel_bounds_min, const glm::vec3& voxel_bounds_max) const;

    TextureHandle get_texture() const;

private:
    uint32_t resolution = 0;
    
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

    /**
     * Events that let us split up the binning workload to achieve greater occupancy
     */
    VkEvent top_half_event = VK_NULL_HANDLE;
    VkEvent bottom_half_event = VK_NULL_HANDLE;

    RenderBackend* backend;

    RenderScene* scene;
    MeshStorage* meshes;
};
