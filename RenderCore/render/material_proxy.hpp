#pragma once

#include <unordered_map>
#include <utility>

#include <volk.h>

#include "render/basic_pbr_material.hpp"
#include "render/scene_pass_type.hpp"
#include "render/backend/pipeline.hpp"

class RenderBackend;

/**
 * Proxy for a material
 *
 * Immutable once created. To bind new resources, destroy this proxy and make a new one
 */
struct MaterialProxy {
    std::unordered_map<ScenePassType, Pipeline> pipelines;

    VkDescriptorSet descriptor_set;
};

using BasicPbrMaterialProxy = std::pair<BasicPbrMaterial, MaterialProxy>;
