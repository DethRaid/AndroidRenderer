#pragma once

#include <volk.h>

#include "core/object_pool.hpp"
#include "render/backend/device_address.hpp"
#include "render/backend/handles.hpp"

struct AccelerationStructure {
    AccelerationStructure() = default;

    VkAccelerationStructureKHR acceleration_structure = VK_NULL_HANDLE;

    DeviceAddress as_address = {};

    BufferHandle buffer = {};

    uint64_t scratch_buffer_size = 0;

    uint32_t num_triangles = 0;
};

using AccelerationStructureHandle = PooledObject<AccelerationStructure>;
