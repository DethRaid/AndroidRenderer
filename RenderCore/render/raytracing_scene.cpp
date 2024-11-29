#include "raytracing_scene.hpp"

#include "render/render_scene.hpp"
#include "backend/render_backend.hpp"
#include "console/cvars.hpp"
#include "render/mesh_storage.hpp"

static auto cvar_enable_raytracing = AutoCVar_Int{
    "r.Raytracing.Enable", "Whether or not to enable raytracing", 1
};

RaytracingScene::RaytracingScene(RenderScene& scene_in)
    : scene{scene_in} {}

void RaytracingScene::add_primitive(const MeshPrimitiveHandle primitive) {
    if(placed_blases.size() <= primitive.index) {
        placed_blases.resize(primitive.index + 1);
    }

    const auto& model_matrix = primitive->data.model;
    placed_blases[primitive.index] = VkAccelerationStructureInstanceKHR{
        .transform = {
            .matrix = {
                {model_matrix[0][0], model_matrix[0][1], model_matrix[0][2], model_matrix[0][3]},
                {model_matrix[1][0], model_matrix[1][1], model_matrix[1][2], model_matrix[1][3]},
                {model_matrix[2][0], model_matrix[2][1], model_matrix[2][2], model_matrix[2][3]}
            }
        },
        .instanceCustomIndex = primitive.index,
        .mask = 0xFF,
        .instanceShaderBindingTableRecordOffset = primitive->material.index * ScenePassType::Count,
        .accelerationStructureReference = primitive->mesh->blas->as_address
    };

    is_dirty = true;
}

void RaytracingScene::finalize() {
    commit_tlas_builds();
}

void RaytracingScene::commit_tlas_builds() {
    if(!is_dirty) {
        return;
    }

    ZoneScoped;

    auto& backend = RenderBackend::get();


}
