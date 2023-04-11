#pragma once

#include <filesystem>
#include <span>
#include <unordered_map>

#include <glm/gtc/type_ptr.hpp>
#include <fastgltf_types.hpp>

#include "render/scene_primitive.hpp"
#include "render/texture_type.hpp"
#include "render/material_storage.hpp"

class RenderBackend;
class SceneRenderer;
class RenderScene;

class TextureLoader;

glm::mat4 get_node_to_parent_matrix(const fastgltf::Node& node);

/**
 * Class for a glTF model
 *
 * This class performs a few functions: It loads the glTF model from disk, it imports its data into the render context,
 * and it provides the glTF data in a runtime-friendly way
 */
class GltfModel {
public:
    GltfModel(std::filesystem::path filepath_in, std::unique_ptr<fastgltf::Asset>&& model, SceneRenderer& renderer);

    glm::vec4 get_bounding_sphere() const;

    const fastgltf::Asset& get_gltf_data() const;

    /**
     * Depth-first traversal of the node hierarchy
     *
     * TraversalFunction must have the following type signature: void(tinygltf::Node& node, const glm::mat4& local_to_world)
     */
    template <typename TraversalFunction>
    void traverse_nodes(TraversalFunction&& traversal_function) const;

    /**
     * Adds the primitives from this model to the primitive scene
     *
     * @param scene Storage to add primitives to
     */
    void add_primitives(RenderScene& scene, RenderBackend& backend);

    void add_to_scene(RenderScene& scene, SceneRenderer& scene_renderer);

private:
    std::filesystem::path filepath;

    std::unique_ptr<fastgltf::Asset> model;
     
    std::unordered_map<size_t, TextureHandle> gltf_texture_to_texture_handle;

    std::vector<PooledObject<BasicPbrMaterialProxy>> gltf_material_to_material_handle;

    // Outer vector is the mesh, inner vector is the primitives within that mesh
    std::vector<std::vector<MeshHandle>> gltf_primitive_to_mesh_primitive;

    /**
     * All the MeshPrimitives that came from this glTF model
     */
    std::vector<PooledObject<MeshPrimitive>> scene_primitives;

    glm::vec4 bounding_sphere = {};

#pragma region init

    void import_resources_for_model(SceneRenderer& renderer);

    void import_materials(MaterialStorage& material_storage, TextureLoader& texture_loader, RenderBackend& backend);

    void import_meshes(SceneRenderer& renderer);

    void calculate_bounding_sphere_and_footprint();

#pragma endregion

    template <typename TraversalFunction>
    void
    visit_node(TraversalFunction&& traversal_function, const fastgltf::Node& node, glm::mat4 parent_to_world) const;

    TextureHandle get_texture(size_t gltf_texture_index, TextureType type, TextureLoader& texture_storage);

    void import_single_texture(size_t gltf_texture_index, TextureType type, TextureLoader& texture_storage);

    static VkSampler to_vk_sampler(const fastgltf::Sampler& sampler, RenderBackend& backend);
};

template <typename TraversalFunction>
void GltfModel::traverse_nodes(TraversalFunction&& traversal_function) const {
    const auto& scene = model->scenes[*model->defaultScene];
    for (const auto& node : scene.nodeIndices) {
        visit_node(traversal_function, model->nodes[node], glm::mat4{1.f});
    }
}

template <typename TraversalFunction>
void GltfModel::visit_node(TraversalFunction&& traversal_function, const fastgltf::Node& node,
                           const glm::mat4 parent_to_world) const {
    const auto local_to_parent = get_node_to_parent_matrix(node);
    const auto local_to_world = parent_to_world * local_to_parent;

    traversal_function(node, local_to_world);

    for (const auto& child_node : node.children) {
        visit_node(traversal_function, model->nodes[child_node], local_to_world);
    }
}
