#pragma once

#include <cstring>

#include <volk.h>

#include "render/backend/device_address.hpp"
#include "render/backend/handles.hpp"

struct AccelerationStructure {
    AccelerationStructure() = default;

    VkAccelerationStructureKHR acceleration_structure = VK_NULL_HANDLE;

    DeviceAddress as_address = {};

    BufferHandle buffer = {};

    uint64_t scratch_buffer_size = 0;

    uint32_t num_triangles = 0;

    bool operator==(const AccelerationStructure& other) const;
};

inline bool AccelerationStructure::operator==(const AccelerationStructure& other) const {
    return memcmp(this, &other, sizeof(AccelerationStructure)) == 0;
}
