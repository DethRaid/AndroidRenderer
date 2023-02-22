#ifndef SAHRENDERER_SCENE_PRIMITIVE_HPP
#define SAHRENDERER_SCENE_PRIMITIVE_HPP

#include <glm/glm.hpp>

#include "core/object_pool.hpp"
#include "render/mesh.hpp"
#include "render/material_proxy.hpp"

struct PrimitiveData {
    glm::mat4 model_matrix;
};

struct MeshPrimitive {
    PrimitiveData data;

    Mesh mesh;

    PooledObject<BasicPbrMaterialProxy> material;
};

#endif //SAHRENDERER_SCENE_PRIMITIVE_HPP
