#include "mesh_drawer.hpp"

#include "indirect_drawing_utils.hpp"
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

    if (is_color_pass(type)) {
        commands.bind_descriptor_set(1, commands.get_backend().get_texture_descriptor_pool().get_descriptor_set());
    }
    
    for (const auto& primitive : solids) {
        const auto& mesh = primitive->mesh;

        commands.bind_pipeline(primitive->material->second.pipelines[static_cast<size_t>(type)]);
        commands.set_push_constant(0, primitive.index);
        commands.draw_indexed(mesh->num_indices, 1, static_cast<uint32_t>(mesh->first_index), static_cast<uint32_t>(mesh->first_vertex), 0);
    }
    
    if (is_color_pass(type)) {
        commands.clear_descriptor_set(1);
    }
}

void SceneDrawer::draw(CommandBuffer& commands, GraphicsPipelineHandle solid_pso) const {
    if (scene == nullptr) {
        return;
    }

    const auto& solids = scene->get_solid_primitives();

    commands.bind_vertex_buffer(0, mesh_storage->get_vertex_position_buffer());
    commands.bind_vertex_buffer(1, mesh_storage->get_vertex_data_buffer());
    commands.bind_index_buffer(mesh_storage->get_index_buffer());

    if (is_color_pass(type)) {
        commands.bind_descriptor_set(1, commands.get_backend().get_texture_descriptor_pool().get_descriptor_set());
    }

    commands.bind_pipeline(solid_pso);

    for (const auto& primitive : solids) {
        const auto& mesh = primitive->mesh;

        commands.set_push_constant(0, primitive.index);
        commands.draw_indexed(mesh->num_indices, 1, static_cast<uint32_t>(mesh->first_index), static_cast<uint32_t>(mesh->first_vertex), 0);
    }

    if (is_color_pass(type)) {
        commands.clear_descriptor_set(1);
    }
}

void SceneDrawer::draw_indirect(
    CommandBuffer& commands, const GraphicsPipelineHandle pso, const IndirectDrawingBuffers& drawbuffers
) const {
    if (scene == nullptr) {
        return;
    }

    const auto& solids = scene->get_solid_primitives();

    commands.bind_vertex_buffer(0, mesh_storage->get_vertex_position_buffer());
    commands.bind_vertex_buffer(1, mesh_storage->get_vertex_data_buffer());
    commands.bind_vertex_buffer(2, drawbuffers.primitive_ids);
    commands.bind_index_buffer(mesh_storage->get_index_buffer());

    if (is_color_pass(type)) {
        commands.bind_descriptor_set(1, commands.get_backend().get_texture_descriptor_pool().get_descriptor_set());
    }

    commands.bind_pipeline(pso);
    commands.draw_indexed_indirect(drawbuffers.commands, drawbuffers.count, static_cast<uint32_t>(solids.size()));

    if (is_color_pass(type)) {
        commands.clear_descriptor_set(1);
    }

}

const RenderScene& SceneDrawer::get_scene() const { return *scene; }

const MeshStorage& SceneDrawer::get_mesh_storage() const { return *mesh_storage; }

const MaterialStorage& SceneDrawer::get_material_storage() const { return *material_storage; }
