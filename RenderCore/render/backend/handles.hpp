#pragma once

#include <cstdint>

using BufferHandle = struct GpuBuffer*;

using TextureHandle = struct GpuTexture*;

using AccelerationStructureHandle = struct AccelerationStructure*;

using GraphicsPipelineHandle = class GraphicsPipeline*;

using ComputePipelineHandle = struct ComputeShader*;
