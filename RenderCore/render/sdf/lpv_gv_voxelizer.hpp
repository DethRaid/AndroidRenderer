#pragma once

#include <glm/vec3.hpp>

#include "render/mesh_drawer.hpp"
#include "render/backend/handles.hpp"
#include "render/backend/pipeline.hpp"

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
    explicit LpvGvVoxelizer(RenderBackend& backend_in, glm::uvec3 voxel_texture_resolution);

    ~LpvGvVoxelizer();

    void set_view(SceneDrawer&& view);

    void voxelize_scene(RenderGraph& graph);

    TextureHandle get_texture() const;

private:
    TextureHandle volume_handle = TextureHandle::None;

    RenderBackend& backend;

    SceneDrawer drawer;

    Pipeline pipeline;
};
