#pragma once

#include "backend/graphics_pipeline.hpp"
#include "backend/handles.hpp"
#include "render/scene_pass_type.hpp"

struct IndirectDrawingBuffers;
class ResourceAllocator;
class MaterialStorage;
class CommandBuffer;
class MeshStorage;
class RenderScene;

/**
 * Draws meshes!
 *
 * RenderScenes can create these and hand them out to other code. These can be used to cull meshes for a viewport
 * and draw those meshes
 *
 * Basically a wrapper around a for loop. Might eventually have some culling logic and more advanced object dispatch -
 * instancing, indirect rendering, etc
 *
 * We need some assumptions for this to work. This code uses descriptor set 1 for material information, so descriptor
 * set 0 is available. Recommended usage is to bind the view's information to set 0.
 *
 * This code also uses push constant 0 for the primitive index. Other push constants are available for external code to
 * set
 */
class SceneDrawer {
public:
    SceneDrawer() = default;

    SceneDrawer(ScenePassType::Type type_in, const RenderScene& scene_in, const MeshStorage& mesh_storage_in, const MaterialStorage& material_storage_in, ResourceAllocator& resource_allocator_in);

    SceneDrawer(const SceneDrawer& other) = default;
    SceneDrawer& operator=(const SceneDrawer& other) = default;

    SceneDrawer(SceneDrawer&& old) noexcept = default;
    SceneDrawer& operator=(SceneDrawer&& old) noexcept = default;

    ~SceneDrawer() = default;

    /**
     * \brief Draws the primitives in the scene using non-indexed draws
     *
     * Note: The PSOs for the type of pass that this scene drawer draws must support non-indexed draws. Currently this
     * is only the shadow pass, eventually it will be nothing
     *
     * \param commands Command buffer to use to render
     */
    void draw(CommandBuffer& commands) const;

    void draw_indirect(CommandBuffer& commands, GraphicsPipelineHandle pso, const IndirectDrawingBuffers& drawbuffers) const;

    const RenderScene& get_scene() const;

    const MeshStorage& get_mesh_storage() const;
    
private: 
    const RenderScene* scene = nullptr;

    const MeshStorage* mesh_storage = nullptr;

    const MaterialStorage* material_storage = nullptr;

    ResourceAllocator* allocator = nullptr;

    ScenePassType::Type type;
};

