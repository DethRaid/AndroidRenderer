#pragma once

#include <unordered_map>
#include <utility>

#include <volk.h>

#include "render/basic_pbr_material.hpp"
#include "render/scene_pass_type.hpp"
#include "render/backend/graphics_pipeline.hpp"

class RenderBackend;

/**
 * Proxy for a material
 */
struct MaterialProxy {
    std::array<GraphicsPipelineHandle, static_cast<uint32_t>(ScenePassType::Count)> pipelines;
};

using BasicPbrMaterialProxy = std::pair<BasicPbrMaterial, MaterialProxy>;
