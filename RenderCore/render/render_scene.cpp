#include "render_scene.hpp"

#include "raytracing_scene.hpp"
#include "backend/pipeline_cache.hpp"
#include "render/backend/resource_allocator.hpp"
#include "render/backend/render_backend.hpp"
#include "console/cvars.hpp"
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
    // sun.set_direction({0.1f, -1.f, 0.33f});
    sun.set_direction({0.1f, -1.f, -0.33f});
    sun.set_color(glm::vec4{1.f, 1.f, 1.f, 0.f} * 100000.f);

    if(*CVarSystem::Get()->GetIntCVar("r.Raytracing.Enable")) {
        raytracing_scene.emplace(RaytracingScene{*this});
    }

    auto& pipeline_cache = backend.get_pipeline_cache();
    emissive_point_cloud_shader = pipeline_cache.create_pipeline("shaders/util/emissive_point_cloud.comp.spv");

    if(*CVarSystem::Get()->GetIntCVar("r.voxel.Enable") != 0) {
        create_voxel_cache();
    }
}

MeshPrimitiveHandle
RenderScene::add_primitive(RenderGraph& graph, MeshPrimitive primitive) {
    const auto materials_buffer = materials.get_material_buffer();
    primitive.data.material_id = materials_buffer->address;
    primitive.data.material_id += sizeof(BasicPbrMaterialGpu) * primitive.material.index;
    primitive.data.mesh_id = primitive.mesh.index;
    primitive.data.type = static_cast<uint32_t>(primitive.material->first.transparency_mode);

    auto handle = mesh_primitives.add_object(std::move(primitive));


    total_num_primitives++;

    switch(handle->material->first.transparency_mode) {
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

void RenderScene::pre_frame(RenderGraph& graph) {
    primitive_upload_buffer.flush_to_buffer(graph, primitive_data_buffer);

    if(*CVarSystem::Get()->GetIntCVar("r.voxel.Enable") != 0) {
        auto& backend = RenderBackend::get();
        auto& texture_descriptors = backend.get_texture_descriptor_pool();

        for(auto& handle : new_primitives) {
            const auto obj = voxel_cache->build_voxels_for_mesh(
                handle,
                meshes,
                primitive_data_buffer,
                graph
            );

            handle->data.voxels_color_srv = texture_descriptors.create_texture_srv(
                obj.voxels_color,
                voxel_sampler
            );
            handle->data.voxels_normal_srv = texture_descriptors.create_texture_srv(obj.voxels_color, voxel_sampler);
            handle->data.voxel_size_xy = glm::u16vec2{obj.worldspace_size.x, obj.worldspace_size.y};
            handle->data.voxel_size_zw = glm::u16vec2{obj.worldspace_size.z, 0u};

            primitive_upload_buffer.add_data(handle.index, handle->data);
        }
    }

    // Gotta flush it again now that we have the SRVs. Kinda annoying but all well
    primitive_upload_buffer.flush_to_buffer(graph, primitive_data_buffer);

    if(raytracing_scene) {
        raytracing_scene->finalize();
    }

    new_primitives.clear();
}

const std::vector<PooledObject<MeshPrimitive>>& RenderScene::get_solid_primitives() const {
    return solid_primitives;
}

const std::vector<MeshPrimitiveHandle>& RenderScene::get_masked_primitives() const {
    return cutout_primitives;
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

const MeshStorage& RenderScene::get_meshes() const {
    return meshes;
}

RaytracingScene& RenderScene::get_raytracing_scene() {
    return *raytracing_scene;
}

VoxelCache& RenderScene::get_voxel_cache() const {
    return *voxel_cache;
}

void RenderScene::create_voxel_cache() {
    auto& backend = RenderBackend::get();
    voxel_cache = std::make_unique<VoxelCache>(backend);

    voxel_sampler = backend.get_global_allocator().get_sampler(
        {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = 16.f,
            .maxLod = 16.f,
        }
    );
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
                    {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT}
                },
                {
                    primitive->mesh->point_cloud_buffer,
                    {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT}
                },
                {
                    primitive_data_buffer,
                    {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT}
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
