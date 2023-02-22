#include "gbuffer_phase.hpp"

#include "render/backend/command_buffer.hpp"
#include "render/scene_renderer.hpp"
#include "render/render_scene.hpp"

GbufferPhase::GbufferPhase(SceneRenderer& scene_renderer_in) : scene_renderer{scene_renderer_in} {}

void GbufferPhase::set_scene(RenderScene& scene_in) {
    scene= &scene_in;
}

void GbufferPhase::render(CommandBuffer& commands, SceneView& view) {
    if(scene == nullptr) {
        return;
    }

    ZoneScoped;

    GpuZoneScoped(commands);

    // Pull drawcalls from the scene

    auto& backend = scene_renderer.get_backend();

    auto global_set = VkDescriptorSet{};
    backend.create_frame_descriptor_builder()
           .bind_buffer(0, {
                   .buffer = view.get_buffer()
           }, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
           .bind_buffer(1, {
                   .buffer = scene->get_primitive_buffer(),
           }, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
           .build(global_set);

    commands.bind_descriptor_set(0, global_set);

    const auto solids = scene->get_solid_primitives();

    auto& mesh_storage = scene_renderer.get_mesh_storage();
    commands.bind_vertex_buffer(0, mesh_storage.get_vertex_position_buffer());
    commands.bind_vertex_buffer(1, mesh_storage.get_vertex_data_buffer());
    commands.bind_index_buffer(mesh_storage.get_index_buffer());

    for (const auto& primitive: solids) {
        // View uniform buffer and primitive data buffer

        commands.bind_descriptor_set(1, primitive->material->second.descriptor_set);

        commands.set_push_constant(0, primitive.index);

        commands.bind_pipeline(primitive->material->first.pipeline);

        const auto& mesh = primitive->mesh;
        commands.draw_indexed(mesh.num_indices, 1, mesh.first_index, mesh.first_vertex, 0);

        commands.clear_descriptor_set(1);
    }

    commands.clear_descriptor_set(0);
}
