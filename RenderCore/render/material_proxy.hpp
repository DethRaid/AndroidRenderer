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
    std::unordered_map<ScenePassType, GraphicsPipelineHandle> pipelines;
};

using BasicPbrMaterialProxy = std::pair<BasicPbrMaterial, MaterialProxy>;
