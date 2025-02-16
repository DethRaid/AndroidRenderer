#pragma once

#include <cstdint>

using BufferHandle = struct GpuBuffer*;

using TextureHandle = struct GpuTexture*;

using AccelerationStructureHandle = struct AccelerationStructure*;

enum class VoxelObjectHandle : uint32_t { None = 0xFFFFFFFF };
