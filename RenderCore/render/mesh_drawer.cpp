#include "mesh_drawer.hpp"

#include "render/material_storage.hpp"
#include "render/mesh_storage.hpp"
#include "render/render_scene.hpp"
#include "render/backend/command_buffer.hpp"

SceneDrawer::SceneDrawer(
    const ScenePassType::Type type_in, const RenderScene& scene_in, const MeshStorage& mesh_storage_in,
    const MaterialStorage& material_storage_in, ResourceAllocator& resource_allocator_in
) : scene{&scene_in}, mesh_storage{&mesh_storage_in}, material_storage{&material_storage_in},
    allocator{&resource_allocator_in}, type{type_in} {}

void SceneDrawer::draw(CommandBuffer& commands) const {
    if (scene == nullptr) {
        return;
    }

    const auto& solids = scene->get_solid_primitives();

    commands.bind_vertex_buffer(0, mesh_storage->get_vertex_position_buffer());
    commands.bind_vertex_buffer(1, mesh_storage->get_vertex_data_buffer());
    commands.bind_index_buffer(mesh_storage->get_index_buffer());

    commands.bind_buffer_reference(0, scene->get_primitive_buffer());

    if (is_color_pass(type)) {
        commands.bind_descriptor_set(1, commands.get_backend().get_texture_descriptor_pool().get_descriptor_set());
    }
    
    for (const auto& primitive : solids) {
        const auto& mesh = primitive->mesh;

        commands.bind_pipeline(primitive->material->second.pipelines[static_cast<size_t>(type)]);
        commands.set_push_constant(2, primitive.index);
        commands.draw_indexed(mesh->num_indices, 1, static_cast<uint32_t>(mesh->first_index), static_cast<uint32_t>(mesh->first_vertex), 0);
    }
    
    if (is_color_pass(type)) {
        commands.clear_descriptor_set(1);
    }
}

void SceneDrawer::draw_indirect(
    CommandBuffer& commands, const BufferHandle indirect_buffer, const BufferHandle draw_count_buffer,
    const BufferHandle primitive_ids
) const {
    if (scene == nullptr) {
        return;
    }

    commands.bind_vertex_buffer(0, mesh_storage->get_vertex_position_buffer());
    commands.bind_vertex_buffer(1, mesh_storage->get_vertex_data_buffer());
    commands.bind_vertex_buffer(2, primitive_ids);
    commands.bind_index_buffer(mesh_storage->get_index_buffer());

    commands.bind_buffer_reference(0, scene->get_primitive_buffer());

    if (is_color_pass(type)) {
        commands.bind_descriptor_set(1, commands.get_backend().get_texture_descriptor_pool().get_descriptor_set());
    }

    const auto& solids = scene->get_solid_primitives();
    if (!solids.empty()) {
        // Assume all the pipelines are the same - because they are
        // TODO: Provide a better way to classify draws by material

        commands.bind_pipeline(solids[0]->material->second.pipelines[static_cast<size_t>(type)]);
        commands.draw_indexed_indirect(indirect_buffer, draw_count_buffer, static_cast<uint32_t>(solids.size()));
    }

    if (is_color_pass(type)) {
        commands.clear_descriptor_set(1);
    }

}

const RenderScene& SceneDrawer::get_scene() const { return *scene; }

const MeshStorage& SceneDrawer::get_mesh_storage() const { return *mesh_storage; }
