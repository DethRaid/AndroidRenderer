#pragma once

#include <concepts>

#include <filesystem>
#include <span>
#include <unordered_map>

#include <glm/gtc/type_ptr.hpp>
#include <tiny_gltf.h>

#include "render/scene_primitive.hpp"
#include "render/texture_type.hpp"
#include "render/material_storage.hpp"

class RenderBackend;
class SceneRenderer;
class RenderScene;

class TextureLoader;

glm::mat4 get_node_to_parent_matrix(const tinygltf::Node& node);

/**
 * Class for a glTF model
 *
 * This class performs a few functions: It loads the glTF model from disk, it imports its data into the render context,
 * and it provides the glTF data in a runtime-friendly way
 */
class GltfModel {
public:
    GltfModel(std::filesystem::path filepath, tinygltf::Model model, SceneRenderer& renderer);

    glm::vec4 get_bounding_sphere() const;

    const tinygltf::Model& get_gltf_data() const;

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

private:
    std::filesystem::path filepath;

    tinygltf::Model model;

    std::unordered_map<int, TextureHandle> gltf_texture_to_texture_handle;

    std::vector<PooledObject<BasicPbrMaterialProxy>> gltf_material_to_material_handle;

    // Outer vector is the mesh, inner vector is the primitives within that mesh
    std::vector<std::vector<Mesh>> gltf_primitive_to_mesh_primitive;

    std::unordered_map<unsigned long, std::vector<PooledObject<MeshPrimitive>>> gltf_primitive_to_scene_primitive;

    glm::vec4 bounding_sphere = {};

#pragma region init

    void import_resources_for_model(SceneRenderer& renderer);

    void import_materials(MaterialStorage& material_storage, TextureLoader& texture_loader, RenderBackend& backend);

    void import_meshes(SceneRenderer& renderer);

    void calculate_bounding_sphere_and_footprint();

#pragma endregion

    template <typename TraversalFunction>
    void
    visit_node(TraversalFunction&& traversal_function, const tinygltf::Node& node, glm::mat4 parent_to_world) const;

    TextureHandle get_texture(int gltf_texture_index, TextureType type, TextureLoader& texture_storage);

    void import_single_texture(int gltf_texture_index, TextureType type, TextureLoader& texture_storage);

    static VkSampler to_vk_sampler(const tinygltf::Sampler& sampler, RenderBackend& backend);
};

template <typename TraversalFunction>
void GltfModel::traverse_nodes(TraversalFunction&& traversal_function) const {
    const auto& scene = model.scenes[model.defaultScene];
    for (const auto& node : scene.nodes) {
        visit_node(traversal_function, model.nodes[node], glm::mat4{1.f});
    }
}

template <typename TraversalFunction>
void GltfModel::visit_node(TraversalFunction&& traversal_function, const tinygltf::Node& node,
                           const glm::mat4 parent_to_world) const {
    const auto local_to_parent = get_node_to_parent_matrix(node);
    const auto local_to_world = parent_to_world * local_to_parent;

    traversal_function(node, local_to_world);

    for (const auto& child_node : node.children) {
        visit_node(traversal_function, model.nodes[child_node], local_to_world);
    }
}
