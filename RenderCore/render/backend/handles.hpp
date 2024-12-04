#pragma once

#include <cstdint>

using BufferHandle = struct GpuBuffer*;

using TextureHandle = struct GpuTexture*;

enum class VoxelObjectHandle : uint32_t { None = 0xFFFFFFFF };
