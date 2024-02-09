#pragma once

#include "render/scene_primitive.hpp"

class RenderScene;

class RaytracingScene {
public:
    explicit RaytracingScene(RenderBackend& backend_in, RenderScene& scene_in);

    void create_blas_for_mesh(MeshHandle mesh);

    void add_primitive(MeshPrimitiveHandle primitive);

    /**
     * \brief Make the raytracing scene ready for raytracing by making sure that all raytraing acceleration structure
     * changes are submitted to the GPU
     *
     * This is basically a barrier from raytracing acceleration structure build commands submit -> raytracing
     * acceleration structures available for raytracing
     */
    void finalize();

private:
    RenderBackend& backend;

    RenderScene& scene;

    VkAccelerationStructureKHR opaque_scene;

    std::vector<VkAccelerationStructureGeometryKHR> pending_blas_geometries;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> pending_blas_ranges;

    /**
     * \brief Makes all the added meshes available to the TLAS build function. Must be called before the TLAS build function
     */
    void commit_blas_builds();

    /**
     * \brief Finishes the raytracing scene by committing pending TLAS builds. Called by finalize()
     */
    void commit_tlas_builds();
};
