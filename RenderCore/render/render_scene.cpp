#include "render_scene.hpp"

#include "indirect_drawing_utils.hpp"
#include "mesh_storage.hpp"
#include "raytracing_scene.hpp"
#include "backend/pipeline_cache.hpp"
#include "render/backend/resource_allocator.hpp"
#include "render/backend/render_backend.hpp"
#include "core/box.hpp"
#include "model_import/gltf_model.hpp"

constexpr uint32_t MAX_NUM_PRIMITIVES = 65536;

RenderScene::RenderScene(MeshStorage& meshes_in, MaterialStorage& materials_in)
    : meshes{meshes_in}, materials{materials_in} {
    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();
    primitive_data_buffer = allocator.create_buffer(
        "Primitive data",
        MAX_NUM_PRIMITIVES * sizeof(PrimitiveDataGPU),
        BufferUsage::StorageBuffer
    );

    // Defaults
    sun.set_direction({0.1f, -1.f, -0.33f});
    // sun.set_direction({0.1f, -1.f, -0.01f});
    sun.set_color(glm::vec4{1.f, 1.f, 1.f, 0.f} * 100000.f);

    if(backend.supports_ray_tracing()) {
        raytracing_scene.emplace(RaytracingScene{*this});
    }

    auto& pipeline_cache = backend.get_pipeline_cache();
    emissive_point_cloud_shader = pipeline_cache.create_pipeline("shaders/util/emissive_point_cloud.comp.spv");
}

MeshPrimitiveHandle
RenderScene::add_primitive(RenderGraph& graph, MeshPrimitive primitive) {
    primitive.data.material_id = primitive.material.index;
    primitive.data.mesh_id = primitive.mesh.index;
    primitive.data.type = static_cast<uint32_t>(primitive.material->first.transparency_mode);

    auto handle = mesh_primitives.add_object(std::move(primitive));

    total_num_primitives++;

    switch(handle->material->first.transparency_mode) {
    case TransparencyMode::Solid:
        solid_primitives.push_back(handle);
        break;

    case TransparencyMode::Cutout:
        masked_primitives.push_back(handle);
        break;

    case TransparencyMode::Translucent:
        translucent_primitives.push_back(handle);
        break;
    }

    if(handle->material->first.emissive) {
        new_emissive_objects.push_back(handle);
    }

    raytracing_scene.map(
        [&](RaytracingScene& rt_scene) {
            rt_scene.add_primitive(handle);
        });

    if(primitive_upload_buffer.is_full()) {
        primitive_upload_buffer.flush_to_buffer(graph, primitive_data_buffer);
    }
    primitive_upload_buffer.add_data(handle.index, handle->data);

    new_primitives.push_back(handle);

    return handle;
}

void RenderScene::begin_frame(RenderGraph& graph) {
    graph.begin_label("RenderScene::pre_frame");

    primitive_upload_buffer.flush_to_buffer(graph, primitive_data_buffer);

    if(raytracing_scene) {
        raytracing_scene->finalize(graph);
    }

    new_primitives.clear();

    graph.end_label();
}

const std::vector<PooledObject<MeshPrimitive>>& RenderScene::get_solid_primitives() const {
    return solid_primitives;
}

const std::vector<MeshPrimitiveHandle>& RenderScene::get_masked_primitives() const {
    return masked_primitives;
}

const std::vector<MeshPrimitiveHandle>& RenderScene::get_transparent_primitives() const {
    return translucent_primitives;
}

BufferHandle RenderScene::get_primitive_buffer() const {
    return primitive_data_buffer;
}

uint32_t RenderScene::get_total_num_primitives() const {
    return total_num_primitives;
}

DirectionalLight& RenderScene::get_sun_light() {
    return sun;
}

std::vector<PooledObject<MeshPrimitive>> RenderScene::get_primitives_in_bounds(
    const glm::vec3& min_bounds, const glm::vec3& max_bounds
) const {
    auto output = std::vector<PooledObject<MeshPrimitive>>{};
    output.reserve(solid_primitives.size());

    const auto test_box = Box{.min = min_bounds, .max = max_bounds};
    for(const auto& primitive : solid_primitives) {
        const auto matrix = primitive->data.model;
        const auto mesh_bounds = primitive->mesh->bounds;

        const auto max_mesh_bounds = mesh_bounds.max;
        const auto min_mesh_bounds = mesh_bounds.min;
        const auto min_primitive_bounds = matrix * glm::vec4{min_mesh_bounds, 1.f};
        const auto max_primitive_bounds = matrix * glm::vec4{max_mesh_bounds, 1.f};

        const auto primitive_box = Box{.min = min_primitive_bounds, .max = max_primitive_bounds};

        if(test_box.overlaps(primitive_box)) {
            output.push_back(primitive);
        }
    }

    return output;
}

void RenderScene::generate_emissive_point_clouds(RenderGraph& render_graph) {
    render_graph.begin_label("Generate emissive mesh VPLs");
    for(auto& primitive : new_emissive_objects) {
        primitive->emissive_points_buffer = generate_vpls_for_primitive(render_graph, primitive);
    }
    render_graph.end_label();

    new_emissive_objects.clear();
}

void RenderScene::draw_opaque(CommandBuffer& commands, const GraphicsPipelineHandle pso) const {
    draw_primitives(commands, pso, solid_primitives);
}

void RenderScene::draw_masked(CommandBuffer& commands, const GraphicsPipelineHandle pso) const {
    draw_primitives(commands, pso, masked_primitives);
}

void RenderScene::draw_opaque(
    CommandBuffer& commands, const IndirectDrawingBuffers& drawbuffers, const GraphicsPipelineHandle solid_pso
) const {
    meshes.bind_to_commands(commands);
    commands.bind_vertex_buffer(2, drawbuffers.primitive_ids);

    if(solid_pso->descriptor_sets.size() > 1) {
        commands.bind_descriptor_set(1, commands.get_backend().get_texture_descriptor_pool().get_descriptor_set());
    }

    commands.bind_pipeline(solid_pso);

    commands.set_cull_mode(VK_CULL_MODE_BACK_BIT);
    commands.set_front_face(VK_FRONT_FACE_CLOCKWISE);

    commands.draw_indexed_indirect(
        drawbuffers.commands,
        drawbuffers.count,
        static_cast<uint32_t>(solid_primitives.size()));

    if(solid_pso->descriptor_sets.size() > 1) {
        commands.clear_descriptor_set(1);
    }
}

void RenderScene::draw_masked(
    CommandBuffer& commands, const IndirectDrawingBuffers& drawbuffers, const GraphicsPipelineHandle masked_pso
) const {
    meshes.bind_to_commands(commands);
    commands.bind_vertex_buffer(2, drawbuffers.primitive_ids);

    if (masked_pso->descriptor_sets.size() > 1) {
        commands.bind_descriptor_set(1, commands.get_backend().get_texture_descriptor_pool().get_descriptor_set());
    }

    commands.bind_pipeline(masked_pso);

    commands.set_cull_mode(VK_CULL_MODE_NONE);
    commands.set_front_face(VK_FRONT_FACE_CLOCKWISE);

    commands.draw_indexed_indirect(
        drawbuffers.commands,
        drawbuffers.count,
        static_cast<uint32_t>(masked_primitives.size()));

    if (masked_pso->descriptor_sets.size() > 1) {
        commands.clear_descriptor_set(1);
    }
}

void RenderScene::draw_transparent(CommandBuffer& commands, GraphicsPipelineHandle pso) const {
    draw_primitives(commands, pso, translucent_primitives);
}

const MeshStorage& RenderScene::get_meshes() const {
    return meshes;
}

RaytracingScene& RenderScene::get_raytracing_scene() {
    return *raytracing_scene;
}

const RaytracingScene& RenderScene::get_raytracing_scene() const {
    return *raytracing_scene;
}
MaterialStorage& RenderScene::get_material_storage() const {
    return materials;
}

MeshStorage& RenderScene::get_mesh_storage() const {
    return meshes;
}

BufferHandle RenderScene::generate_vpls_for_primitive(
    RenderGraph& graph, const PooledObject<MeshPrimitive>& primitive
) {
    auto& backend = RenderBackend::get();
    const auto vpl_buffer_handle = backend.get_global_allocator().create_buffer(
        "Primitive emission buffer",
        primitive->mesh->num_points * sizeof(glm::vec4),
        BufferUsage::StorageBuffer
    );

    struct EmissivePointCloudConstants {
        DeviceAddress primitive_data;
        DeviceAddress point_cloud;
        DeviceAddress vpl_buffer;
        uint32_t primitive_index;
        uint32_t num_points;
    };

    graph.add_compute_dispatch(
        ComputeDispatch<EmissivePointCloudConstants>{
            .name = "Build emissive points",
            .descriptor_sets = std::vector{backend.get_texture_descriptor_pool().get_descriptor_set()},
            .buffers = {
                {
                    vpl_buffer_handle,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT
                },
                {
                    primitive->mesh->point_cloud_buffer,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_ACCESS_2_SHADER_READ_BIT
                },
                {
                    primitive_data_buffer,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_ACCESS_2_SHADER_READ_BIT
                },
            },
            .push_constants = {
                .primitive_data = primitive_data_buffer->address,
                .point_cloud = primitive->mesh->point_cloud_buffer->address,
                .vpl_buffer = vpl_buffer_handle->address,
                .primitive_index = primitive.index,
                .num_points = primitive->mesh->num_points,
            },
            .num_workgroups = {
                (primitive->mesh->num_points + 95) / 96, 1, 1
            },
            .compute_shader = emissive_point_cloud_shader
        });

    return vpl_buffer_handle;
}

void RenderScene::draw_primitives(
    CommandBuffer& commands, const GraphicsPipelineHandle pso, const std::span<const MeshPrimitiveHandle> primitives
) const {
    meshes.bind_to_commands(commands);

    if(pso->descriptor_sets.size() > 1) {
        commands.bind_descriptor_set(1, commands.get_backend().get_texture_descriptor_pool().get_descriptor_set());
    }

    commands.bind_pipeline(pso);
    for(const auto& primitive : primitives) {
        const auto& mesh = primitive->mesh;

        if(primitive->material->first.double_sided) {
            commands.set_cull_mode(VK_CULL_MODE_NONE);
        } else {
            commands.set_cull_mode(VK_CULL_MODE_BACK_BIT);
        }

        if(primitive->material->first.front_face_ccw) {
            commands.set_front_face(VK_FRONT_FACE_COUNTER_CLOCKWISE);
        } else {
            commands.set_front_face(VK_FRONT_FACE_CLOCKWISE);
        }

        commands.set_push_constant(0, primitive.index);
        commands.draw_indexed(
            mesh->num_indices,
            1,
            static_cast<uint32_t>(mesh->first_index),
            static_cast<uint32_t>(mesh->first_vertex),
            0);
    }

    if(pso->descriptor_sets.size() > 1) {
        commands.clear_descriptor_set(1);
    }
}
