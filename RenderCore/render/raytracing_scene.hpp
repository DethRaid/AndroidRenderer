#pragma once

#include "render/scene_primitive.hpp"

class RenderScene;

class RaytracingScene {
public:
    explicit RaytracingScene(RenderScene& scene_in);

    void add_primitive(MeshPrimitiveHandle primitive);

    /**
     * \brief Make the raytracing scene ready for raytracing by making sure that all raytraing acceleration structure
     * changes are submitted to the GPU
     *
     * This is basically a barrier from raytracing acceleration structure build commands submit -> raytracing
     * acceleration structures available for raytracing
     */
    void finalize();

    AccelerationStructureHandle get_acceleration_structure() const;

private:
    RenderScene& scene;

    std::vector<VkAccelerationStructureInstanceKHR> placed_blases;
    bool is_dirty = false;
    AccelerationStructureHandle acceleration_structure;

    /**
     * \brief Finishes the raytracing scene by committing pending TLAS builds. Called by finalize()
     */
    void commit_tlas_builds();
};
