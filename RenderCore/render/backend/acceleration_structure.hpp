#pragma once

#include <volk.h>

#include "render/backend/handles.hpp"

struct AccelerationStructure {
    VkAccelerationStructureKHR acceleration_structure;

    BufferHandle buffer;
};
