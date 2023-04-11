#include "render_scene.hpp"

#include "render/backend/resource_allocator.hpp"
#include "render/backend/render_backend.hpp"
#include "console/cvars.hpp"
#include "core/box.hpp"
#include "gltf/gltf_model.hpp"

constexpr const uint32_t max_num_primitives = 65536;

RenderScene::RenderScene(RenderBackend& backend_in)
    : backend{backend_in}, sun{backend}, primitive_upload_buffer{backend_in} {
    auto& allocator = backend.get_global_allocator();
    primitive_data_buffer = allocator.create_buffer(
        "Primitive data",
        max_num_primitives * sizeof(PrimitiveDataGPU),
        BufferUsage::StorageBuffer
    );

    // Defaults
    // sun.set_direction({0.1f, -1.f, 0.33f});
    sun.set_direction({0.1f, -1.f, -0.33f});
    sun.set_color(glm::vec4{1.f, 1.f, 1.f, 0.f} * 80000.f);
}

PooledObject<MeshPrimitive>
RenderScene::add_primitive(RenderGraph& graph, MeshPrimitive primitive) {
    // materials.

    const auto handle = mesh_primitives.add_object(std::move(primitive));

    if (primitive_upload_buffer.is_full()) {
        primitive_upload_buffer.flush_to_buffer(graph, primitive_data_buffer);
    }
    primitive_upload_buffer.add_data(handle.index, handle->data);

    switch (handle->material->first.transparency_mode) {
    case TransparencyMode::Solid:
        solid_primitives.push_back(handle);
        break;

    case TransparencyMode::Cutout:
        cutout_primitives.push_back(handle);
        break;

    case TransparencyMode::Translucent:
        translucent_primitives.push_back(handle);
        break;
    }

    return handle;
}

void RenderScene::flush_primitive_upload(RenderGraph& graph) {
    primitive_upload_buffer.flush_to_buffer(graph, primitive_data_buffer);
}


const std::vector<PooledObject<MeshPrimitive>>& RenderScene::get_solid_primitives() const {
    return solid_primitives;
}

BufferHandle RenderScene::get_primitive_buffer() const {
    return primitive_data_buffer;
}

SunLight& RenderScene::get_sun_light() {
    return sun;
}

std::vector<PooledObject<MeshPrimitive>> RenderScene::get_primitives_in_bounds(
    const glm::vec3& min_bounds, const glm::vec3& max_bounds
) const {
    auto output = std::vector<PooledObject<MeshPrimitive>>{};
    output.reserve(solid_primitives.size());

    const auto test_box = Box{.min = min_bounds, .max = max_bounds};
    for (const auto& primitive : solid_primitives) {
        const auto matrix = primitive->data.model;
        const auto mesh_bounds = primitive->mesh->bounds;

        const auto max_mesh_bounds = mesh_bounds * 0.5f;
        const auto min_mesh_bounds = -max_mesh_bounds;
        const auto min_primitive_bounds = matrix * glm::vec4{min_mesh_bounds, 1.f};
        const auto max_primitive_bounds = matrix * glm::vec4{max_mesh_bounds, 1.f};

        const auto primitive_box = Box{.min = min_primitive_bounds, .max = max_primitive_bounds};

        if (test_box.overlaps(primitive_box)) {
            output.push_back(primitive);
        }
    }

    return output;
}
