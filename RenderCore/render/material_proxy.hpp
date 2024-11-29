#pragma once

#include <array>
#include <utility>

#include "render/basic_pbr_material.hpp"
#include "render/scene_pass_type.hpp"
#include "render/backend/graphics_pipeline.hpp"

class RenderBackend;

/**
 * Proxy for a material
 */
struct MaterialProxy {
    std::array<GraphicsPipelineHandle, ScenePassType::Count> pipelines;
};

using BasicPbrMaterialProxy = std::pair<BasicPbrMaterial, MaterialProxy>;
