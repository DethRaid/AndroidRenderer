#pragma once

#include <memory>

#include "render/scene_pass_type.hpp"

class CommandBuffer;
class MeshStorage;
class RenderScene;
class SceneTransform;

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

    SceneDrawer(const SceneDrawer& other) = default;
    SceneDrawer& operator=(const SceneDrawer& other) = default;

    SceneDrawer(SceneDrawer&& old) noexcept = default;
    SceneDrawer& operator=(SceneDrawer&& old) noexcept = default;

    ~SceneDrawer() = default;

    void draw(CommandBuffer& commands) const;

protected:
    SceneDrawer(ScenePassType type_in, const RenderScene& scene_in, const MeshStorage& mesh_storage_in);
    
    friend class RenderScene;

private: 
    const RenderScene* scene = nullptr;

    const MeshStorage* mesh_storage = nullptr;

    ScenePassType type;
};

