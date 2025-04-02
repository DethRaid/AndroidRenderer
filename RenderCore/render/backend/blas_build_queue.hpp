#pragma once

#include <EASTL/vector.h>

#include "render/backend/acceleration_structure.hpp"

class RenderGraph;

struct BlasBuildJob {
    AccelerationStructureHandle handle;
    VkAccelerationStructureGeometryKHR create_info;
};

class BlasBuildQueue {
public:
    explicit BlasBuildQueue();

    void enqueue(AccelerationStructureHandle blas, const VkAccelerationStructureGeometryKHR& create_info);

    void flush_pending_builds(RenderGraph& graph);

private:
    eastl::vector<BlasBuildJob> pending_jobs;
};
