#include "mesh_drawer.hpp"

#include "render/mesh_storage.hpp"
#include "render/render_scene.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/command_buffer.hpp"

SceneDrawer::SceneDrawer(
    const ScenePassType type_in, const RenderScene& scene_in, const MeshStorage& mesh_storage_in
) : scene{&scene_in}, mesh_storage{&mesh_storage_in}, type{type_in} {}

void SceneDrawer::draw(CommandBuffer& commands) const {
    if (scene == nullptr) {
        return;
    }

    const auto& solids = scene->get_solid_primitives();

    commands.bind_vertex_buffer(0, mesh_storage->get_vertex_position_buffer());
    commands.bind_vertex_buffer(1, mesh_storage->get_vertex_data_buffer());
    commands.bind_index_buffer(mesh_storage->get_index_buffer());

    const auto primitive_data_set = *commands.get_backend().create_frame_descriptor_builder()
                                            .bind_buffer(
                                                0, {.buffer = scene->get_primitive_buffer()},
                                                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT
                                            )
                                            .build();

    commands.bind_descriptor_set(1, primitive_data_set);

    for (const auto& primitive : solids) {
        if (is_color_pass(type)) {
            commands.bind_descriptor_set(2, primitive->material->second.descriptor_set);
        }

        commands.set_push_constant(0, primitive.index);

        commands.bind_pipeline(primitive->material->second.pipelines[type]);

        const auto& mesh = primitive->mesh;
        commands.draw_indexed(mesh->num_indices, 1, mesh->first_index, mesh->first_vertex, 0);

        if (is_color_pass(type)) {
            commands.clear_descriptor_set(2);
        }
    }

    commands.clear_descriptor_set(1);
}
