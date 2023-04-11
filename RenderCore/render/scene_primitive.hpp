#ifndef SAHRENDERER_SCENE_PRIMITIVE_HPP
#define SAHRENDERER_SCENE_PRIMITIVE_HPP

#include <glm/glm.hpp>

#include "render/mesh_handle.hpp"
#include "core/object_pool.hpp"
#include "render/material_proxy.hpp"
#include "shared/primitive_data.hpp"

struct MeshPrimitive {
    PrimitiveDataGPU data = {};

    MeshHandle mesh;

    PooledObject<BasicPbrMaterialProxy> material;
};

#endif //SAHRENDERER_SCENE_PRIMITIVE_HPP
