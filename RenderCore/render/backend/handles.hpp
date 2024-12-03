#pragma once

#include <cstdint>

#include "render/backend/buffer.hpp"
#include "core/object_pool.hpp"

using BufferHandle = PooledObject<GpuBuffer>;

using TextureHandle = struct GpuTexture*;

enum class VoxelObjectHandle : uint32_t { None = 0xFFFFFFFF };
