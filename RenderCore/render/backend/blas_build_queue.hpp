#pragma once

#include <vector>

#include "acceleration_structure.hpp"

class CommandBuffer;
class RenderBackend;

struct BlasBuildJob {
    AccelerationStructureHandle handle;
    VkAccelerationStructureGeometryKHR create_info;
};

class BlasBuildQueue {
public:
    explicit BlasBuildQueue(RenderBackend& backend_in);

    void enqueue(AccelerationStructureHandle blas, const VkAccelerationStructureGeometryKHR& create_info);

    void flush_pending_builds(CommandBuffer& commands);

private:
    RenderBackend& backend;

    std::vector<BlasBuildJob> pending_jobs;
};

