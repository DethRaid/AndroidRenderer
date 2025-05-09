#pragma once

#include "render/procedural_sky.hpp"
#include "render/raytracing_scene.hpp"
#include "core/object_pool.hpp"
#include "render/backend/handles.hpp"
#include "render/scene_primitive.hpp"
#include "render/backend/scatter_upload_buffer.hpp"
#include "render/directional_light.hpp"

struct IndirectDrawingBuffers;
class MaterialStorage;
class MeshStorage;
class GltfModel;
class RenderBackend;

/**
 * A scene that can be rendered!
 *
 * Contains lots of wonderful things - meshes, materials, ray tracing acceleration structure, emissive point clouds, and more!
 */
class RenderScene {
public:
    explicit RenderScene(MeshStorage& meshes_in, MaterialStorage& materials_in);

    MeshPrimitiveHandle add_primitive(RenderGraph& graph, MeshPrimitive primitive);

    void begin_frame(RenderGraph& graph);

    const eastl::vector<MeshPrimitiveHandle>& get_solid_primitives() const;

    const eastl::vector<MeshPrimitiveHandle>& get_masked_primitives() const;

    const eastl::vector<MeshPrimitiveHandle>& get_transparent_primitives() const;

    BufferHandle get_primitive_buffer() const;

    uint32_t get_total_num_primitives() const;

    DirectionalLight& get_sun_light();

    const DirectionalLight& get_sun_light() const;

    const ProceduralSky& get_sky() const;

    /**
     * Retrieves a list of all solid primitives that lie within the given bounds
     */
    eastl::vector<MeshPrimitiveHandle> get_primitives_in_bounds(
        const glm::vec3& min_bounds, const glm::vec3& max_bounds
    ) const;

    /**
     * \brief Generates emissive point clouds for new emissive meshes
     */
    void generate_emissive_point_clouds(RenderGraph& render_graph);

    void draw_opaque(CommandBuffer& commands, GraphicsPipelineHandle pso) const;

    void draw_masked(CommandBuffer& commands, GraphicsPipelineHandle pso) const;

    /**
     * Draws the commands in the IndirectDrawingBuffers with the provided opaque PSO
     */
    void draw_opaque(
        CommandBuffer& commands, const IndirectDrawingBuffers& drawbuffers, GraphicsPipelineHandle solid_pso
    ) const;

    void draw_masked(
        CommandBuffer& commands, const IndirectDrawingBuffers& drawbuffers, GraphicsPipelineHandle masked_pso
    ) const;

    void draw_transparent(CommandBuffer& commands, GraphicsPipelineHandle pso) const;

    const MeshStorage& get_meshes() const;

    RaytracingScene& get_raytracing_scene();

    const RaytracingScene& get_raytracing_scene() const;

    MaterialStorage& get_material_storage() const;

    MeshStorage& get_mesh_storage() const;

private:
    MeshStorage& meshes;

    MaterialStorage& materials;

    tl::optional<RaytracingScene> raytracing_scene;

    DirectionalLight sun;

    ProceduralSky sky;

    ObjectPool<MeshPrimitive> mesh_primitives;

    uint32_t total_num_primitives = 0u;
    BufferHandle primitive_data_buffer;

    ScatterUploadBuffer<PrimitiveDataGPU> primitive_upload_buffer;

    // TODO: Group solid primitives by front face

    eastl::vector<MeshPrimitiveHandle> solid_primitives;

    // TODO: Group masked primitives by front face and cull mode

    eastl::vector<MeshPrimitiveHandle> masked_primitives;

    eastl::vector<MeshPrimitiveHandle> translucent_primitives;

    eastl::vector<MeshPrimitiveHandle> new_emissive_objects;

    ComputePipelineHandle emissive_point_cloud_shader;

    eastl::vector<MeshPrimitiveHandle> new_primitives;

    BufferHandle generate_vpls_for_primitive(RenderGraph& graph, const MeshPrimitiveHandle& primitive);

    void draw_primitives(
        CommandBuffer& commands, GraphicsPipelineHandle pso, std::span<const MeshPrimitiveHandle> primitives
    ) const;
};
