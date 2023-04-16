#include "render_scene.hpp"

#include "render/backend/resource_allocator.hpp"
#include "render/backend/render_backend.hpp"
#include "console/cvars.hpp"
#include "core/box.hpp"
#include "gltf/gltf_model.hpp"

constexpr const uint32_t max_num_primitives = 65536;

RenderScene::RenderScene(RenderBackend& backend_in, MeshStorage& meshes_in, MaterialStorage& materials_in)
    : backend{backend_in}, meshes{meshes_in}, materials{materials_in}, sun{backend},
      primitive_upload_buffer{backend_in} {
    auto& allocator = backend.get_global_allocator();
    primitive_data_buffer = allocator.create_buffer(
        "Primitive data",
        max_num_primitives * sizeof(PrimitiveDataGPU),
        BufferUsage::StorageBuffer
    );

    // Defaults
    // sun.set_direction({0.1f, -1.f, 0.33f});
    sun.set_direction({0.1f, -1.f, -0.33f});
    sun.set_color(glm::vec4{1.f, 1.f, 1.f, 0.f} * 100000.f);


    {
        const auto bytes = *SystemInterface::get().load_file("shaders/util/emissive_point_cloud.comp.spv");
        emissive_point_cloud_shader = *backend.create_compute_shader("Generate emissive point cloud", bytes);
    }
}

PooledObject<MeshPrimitive>
RenderScene::add_primitive(RenderGraph& graph, MeshPrimitive primitive) {
    auto& allocator = backend.get_global_allocator();

    const auto materials_buffer = materials.get_material_buffer();
    const auto& materials_buffer_actual = allocator.get_buffer(materials_buffer);
    primitive.data.material_id = materials_buffer_actual.address;
    primitive.data.material_id.x += sizeof(BasicPbrMaterialGpu) * primitive.material.index;

    auto handle = mesh_primitives.add_object(std::move(primitive));

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

    if(handle->material->first.emissive) {
        new_emissive_objects.push_back(handle);
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

void RenderScene::generate_emissive_point_clouds(RenderGraph& render_graph) {
    render_graph.begin_label("Generate emissive mesh VPLs");
    for(auto& primitive : new_emissive_objects) {
        primitive->emissive_points_buffer = generate_emissive_point_cloud(render_graph, primitive);
    }
    render_graph.end_label();

    new_emissive_objects.clear();
}

BufferHandle RenderScene::generate_emissive_point_cloud(
    RenderGraph& graph, const PooledObject<MeshPrimitive>& primitive
) {
    const auto handle = backend.get_global_allocator().create_buffer(
        "Primitive emission buffer", primitive->mesh->num_points * sizeof(glm::vec4), BufferUsage::StorageBuffer
    );

    graph.add_compute_pass(
        {
            .name = "Build emissive points",
            .buffers = {
                {handle, {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT}},
                {
                    primitive->mesh->point_cloud_buffer,
                    {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT}
                },
                {
                    primitive_data_buffer,
                    {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT}
                }
            },
            .execute = [&](CommandBuffer& commands) {
                commands.bind_buffer_reference(0, primitive_data_buffer);
                commands.bind_buffer_reference(2, primitive->mesh->point_cloud_buffer);
                commands.bind_buffer_reference(4, handle);
                commands.set_push_constant(6, primitive.index);
                commands.set_push_constant(7, primitive->mesh->num_points);

                commands.bind_descriptor_set(0, backend.get_texture_descriptor_pool().get_descriptor_set());

                commands.bind_shader(emissive_point_cloud_shader);

                commands.dispatch((primitive->mesh->num_points + 95) / 96, 1, 1);

                commands.clear_descriptor_set(0);
            }
        }
    );

    return handle;
}
