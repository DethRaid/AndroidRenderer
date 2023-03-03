#include "mesh_drawer.hpp"

#include "render/mesh_storage.hpp"
#include "render/render_scene.hpp"
#include "render/scene_view.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/command_buffer.hpp"

SceneDrawer::SceneDrawer(
    const ScenePassType type_in, const RenderScene& scene_in, const MeshStorage& mesh_storage_in
) : scene{&scene_in}, mesh_storage{&mesh_storage_in}, type{type_in} {}

void SceneDrawer::draw(CommandBuffer& commands) const {
    if (scene == nullptr) {
        return;
    }

    const auto solids = scene->get_solid_primitives();

    commands.bind_vertex_buffer(0, mesh_storage->get_vertex_position_buffer());
    commands.bind_vertex_buffer(1, mesh_storage->get_vertex_data_buffer());
    commands.bind_index_buffer(mesh_storage->get_index_buffer());

    for (const auto& primitive : solids) {
        if (type != ScenePassType::Shadow) {
            commands.bind_descriptor_set(1, primitive->material->second.descriptor_set);
        }

        commands.set_push_constant(0, primitive.index);

        commands.bind_pipeline(primitive->material->first.pipelines[type]);

        const auto& mesh = primitive->mesh;
        commands.draw_indexed(mesh.num_indices, 1, mesh.first_index, mesh.first_vertex, 0);

        if (type != ScenePassType::Shadow) {
            commands.clear_descriptor_set(1);
        }
    }
}
