#include "render_scene.hpp"

#include "render/backend/resource_allocator.hpp"
#include "render/backend/render_backend.hpp"
#include "console/cvars.hpp"

constexpr const uint32_t max_num_primitives = 65536;

RenderScene::RenderScene(RenderBackend &backend_in) : backend{backend_in},
                                                      sun{backend},
                                                      primitive_upload_buffer{backend_in} {
    auto &allocator = backend.get_global_allocator();
    primitive_data_buffer = allocator.create_buffer("Primitive data",
                                                    max_num_primitives * sizeof(PrimitiveData),
                                                    BufferUsage::StorageBuffer);

    // Defaults
    sun.set_direction({0.15f, -1.f, 0.4f});
    sun.set_color(glm::vec4{1.f, 1.f, 1.f, 0.f} * 80000.f);
}

PooledObject<MeshPrimitive>
RenderScene::add_primitive(CommandBuffer &commands, MeshPrimitive primitive) {
    const auto handle = meshes.add_object(std::move(primitive));

    if (primitive_upload_buffer.is_full()) {
        primitive_upload_buffer.flush_to_buffer(commands, primitive_data_buffer);
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

void RenderScene::flush_primitive_upload(CommandBuffer &commands) {
    primitive_upload_buffer.flush_to_buffer(commands, primitive_data_buffer);
}

void RenderScene::add_model(GltfModel &model) {
    model.add_primitives(*this, backend);
}

const std::vector<PooledObject<MeshPrimitive>> &RenderScene::get_solid_primitives() const {
    return solid_primitives;
}

BufferHandle RenderScene::get_primitive_buffer() {
    return primitive_data_buffer;
}

SunLight &RenderScene::get_sun_light() {
    return sun;
}
