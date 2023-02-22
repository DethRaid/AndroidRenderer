#pragma once

#include <utility>

#include <volk.h>

struct BasicPbrMaterial;

class RenderBackend;

/**
 * Proxy for a material
 *
 * Immutable once created. To bind new resources, destroy this proxy and make a new one
 */
struct MaterialProxy {
public:
    static MaterialProxy create(const BasicPbrMaterial& material, RenderBackend& backend);

    VkDescriptorSet descriptor_set;
};

using BasicPbrMaterialProxy = std::pair<BasicPbrMaterial, MaterialProxy>;
