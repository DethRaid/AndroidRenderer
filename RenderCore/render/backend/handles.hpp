#pragma once

#include <cstdint>

#include "render/backend/buffer.hpp"
#include "core/object_pool.hpp"

using BufferHandle = PooledObject<Buffer>;

enum class TextureHandle : uint32_t { None = 0xFFFFFFFF };

enum class VoxelObjectHandle : uint32_t { None = 0xFFFFFFFF };
