#include "gltf_model.hpp"

#include <span>

#include <volk.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <magic_enum.hpp>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/android_sink.h>
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

#include "core/percent_encoding.hpp"
#include "render/basic_pbr_material.hpp"
#include "render/scene_renderer.hpp"
#include "render/standard_vertex.hpp"
#include "render/backend/pipeline.hpp"
#include "render/render_scene.hpp"
#include "render/texture_loader.hpp"

static std::shared_ptr<spdlog::logger> logger;

static std::vector<StandardVertex>
read_vertex_data(const tinygltf::Primitive& primitive, const tinygltf::Model& model);

static std::vector<uint32_t>
read_index_data(const tinygltf::Primitive& primitive, const tinygltf::Model& model);

static void copy_vertex_data_to_vector(
    const std::map<std::string, int>& attributes,
    const tinygltf::Model& model,
    StandardVertex* vertices
);

glm::mat4 get_node_to_parent_matrix(const tinygltf::Node& node) {
    if (!node.matrix.empty()) {
        return glm::make_mat4(node.matrix.data());
    } else {
        glm::mat4 local_matrix(1.0);

        if (!node.translation.empty()) {
            const auto translation = glm::vec3{glm::make_vec3(node.translation.data())};
            local_matrix = glm::translate(local_matrix, translation);
        }
        if (!node.rotation.empty()) {
            auto rotation = glm::make_quat(node.rotation.data());
            local_matrix *= glm::mat4{glm::toMat4(rotation)};
        }
        if (!node.scale.empty()) {
            const auto scale_factors = glm::vec3{glm::make_vec3(node.scale.data())};
            local_matrix = glm::scale(local_matrix, scale_factors);
        }

        return local_matrix;
    }
}

GltfModel::GltfModel(
    std::filesystem::path filepath,
    tinygltf::Model model,
    SceneRenderer& renderer
)
    : filepath{std::move(filepath)}, model{std::move(model)} {
    if (logger == nullptr) {
        logger = SystemInterface::get().get_logger("GltfModel");
    }

    ZoneScoped;

    logger->info("Beginning load of model {}", filepath.string());

    auto loader = tinygltf::TinyGLTF{};

    import_resources_for_model(renderer);

    calculate_bounding_sphere_and_footprint();

    logger->info("Loaded model {}", filepath.string());
}

glm::vec4 GltfModel::get_bounding_sphere() const { return bounding_sphere; }

const tinygltf::Model& GltfModel::get_gltf_data() const { return model; }

void GltfModel::add_primitives(RenderScene& scene, RenderBackend& backend) {
    auto commands = backend.create_command_buffer();
    traverse_nodes(
        [&](const tinygltf::Node& node, const glm::mat4& node_to_world) {
            if (node.mesh != -1) {
                const auto& mesh = model.meshes[node.mesh];
                auto scene_primitives = std::vector<PooledObject<MeshPrimitive>>{};
                scene_primitives.reserve(mesh.primitives.size());
                for (auto i = 0u; i < mesh.primitives.size(); i++) {
                    const auto gltf_primitive = mesh.primitives.at(i);
                    const auto& imported_mesh = gltf_primitive_to_mesh_primitive.at(node.mesh).at(i);
                    const auto& imported_material = gltf_material_to_material_handle.at(
                        gltf_primitive.material
                    );
                    const auto handle = scene.add_primitive(
                        commands, {
                            .data = PrimitiveData{.model_matrix = node_to_world},
                            .mesh = imported_mesh,
                            .material = imported_material,
                        }
                    );
                    scene_primitives.emplace_back(handle);
                }
                const auto node_itr = std::find(model.nodes.begin(), model.nodes.end(), node);
                const auto node_index = node_itr - model.nodes.begin();
                gltf_primitive_to_scene_primitive.emplace(node_index, scene_primitives);
            }
        }
    );
    logger->info("Added nodes to the render scene");
}

void GltfModel::import_resources_for_model(SceneRenderer& renderer) {
    ZoneScoped;

    // Upload all buffers and textures to the GPU, maintaining a mapping from glTF resource identifier to resource
    // Traverse the glTF scene. For each node with a mesh, create a `PlacesMeshPrimitive` with the mesh -> world
    // transformation matrix already calculated
    // Create a mapping from glTF scene to the `PlacesMeshPrimitive` objects it owns, so we can unload the scene

    import_materials(
        renderer.get_material_storage(), renderer.get_texture_loader(),
        renderer.get_backend()
    );

    import_meshes(renderer);

    logger->info("Imported resources");
}

void
GltfModel::import_materials(
    MaterialStorage& material_storage, TextureLoader& texture_loader,
    RenderBackend& backend
) {
    ZoneScoped;

    gltf_material_to_material_handle.clear();
    gltf_material_to_material_handle.reserve(model.materials.size());

    int gltf_idx = 0;
    for (const auto& gltf_material : model.materials) {
        const auto material_name = !gltf_material.name.empty()
                                       ? gltf_material.name
                                       : "Unnamed material";
        logger->info("Importing material {}", material_name);

        // Naive implementation creates a separate material for each glTF material
        // A better implementation would have a few pipeline objects that can be shared - e.g. we'd save the
        // pipeline create info and descriptor set layout info, and copy it down as needed

        auto builder = backend.begin_building_pipeline(material_name)
                              .set_vertex_shader("shaders/deferred/basic.vert.spv")
                              .set_fragment_shader("shaders/deferred/standard_pbr.frag.spv")
                              .set_blend_state(
                                  0,
                                  {
                                      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                      VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT |
                                      VK_COLOR_COMPONENT_A_BIT
                                  }
                              )
                              .set_blend_state(
                                  1,
                                  {
                                      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                      VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT |
                                      VK_COLOR_COMPONENT_A_BIT
                                  }
                              )
                              .set_blend_state(
                                  2,
                                  {
                                      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                      VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT |
                                      VK_COLOR_COMPONENT_A_BIT
                                  }
                              )
                              .set_blend_state(
                                  3,
                                  {
                                      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                      VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT |
                                      VK_COLOR_COMPONENT_A_BIT
                                  }
                              );

        auto material = BasicPbrMaterial{};

        if (gltf_material.alphaMode == "OPAQUE") {
            material.transparency_mode = TransparencyMode::Solid;
        } else if (gltf_material.alphaMode == "MASK") {
            material.transparency_mode = TransparencyMode::Cutout;
        } else if (gltf_material.alphaMode == "BLEND") {
            const auto blend_state = VkPipelineColorBlendAttachmentState{
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            };
            builder.set_blend_state(0, blend_state);
            builder.set_blend_state(1, blend_state);
            builder.set_blend_state(2, blend_state);

            material.transparency_mode = TransparencyMode::Translucent;
        }

        material.shadow_pipeline = backend.begin_building_pipeline(fmt::format("{} SHADOW", material_name))
                                          .set_vertex_shader("shaders/lighting/shadow.vert.spv")
                                          .set_depth_state(
                                              DepthStencilState{
                                                  .compare_op = VK_COMPARE_OP_LESS
                                              }
                                          )
                                          .build();

        material.rsm_pipeline = backend.begin_building_pipeline(fmt::format("{} RSM", material_name))
                                          .set_vertex_shader("shaders/lpv/rsm.vert.spv")
                                          .set_fragment_shader("shaders/lpv/rsm.frag.spv")
                                          .set_depth_state(
                                              DepthStencilState{
                                                  .compare_op = VK_COMPARE_OP_LESS
                                              }
                                          )
                                          .set_blend_state(
                                              0,
                                              {
                                                  .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                                    VK_COLOR_COMPONENT_G_BIT |
                                                                    VK_COLOR_COMPONENT_B_BIT |
                                                                    VK_COLOR_COMPONENT_A_BIT
                                              }
                                          )
                                          .set_blend_state(
                                              1,
                                              {
                                                  .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                                    VK_COLOR_COMPONENT_G_BIT |
                                                                    VK_COLOR_COMPONENT_B_BIT |
                                                                    VK_COLOR_COMPONENT_A_BIT
                                              }
                                          )
                                          .build();

        material.pipeline = builder.build();

        material.double_sided = gltf_material.doubleSided;

        material.gpu_data.base_color_tint = glm::vec4(
            glm::make_vec4(gltf_material.pbrMetallicRoughness.baseColorFactor.data())
        );
        material.gpu_data.metalness_factor = static_cast<float>(gltf_material.pbrMetallicRoughness.metallicFactor);
        material.gpu_data.roughness_factor = static_cast<float>(gltf_material.pbrMetallicRoughness.roughnessFactor);

        if (gltf_material.pbrMetallicRoughness.baseColorTexture.index != -1) {
            material.base_color_texture = get_texture(
                gltf_material.pbrMetallicRoughness.baseColorTexture.index,
                TextureType::Color, texture_loader
            );

            const auto& texture = model.textures[gltf_material.pbrMetallicRoughness.baseColorTexture.index];
            const auto& sampler = model.samplers[texture.sampler];

            material.base_color_sampler = to_vk_sampler(sampler, backend);
        } else {
            material.base_color_texture = backend.get_white_texture_handle();
            material.base_color_sampler = backend.get_default_sampler();
        }

        if (gltf_material.normalTexture.index != -1) {
            material.normal_texture = get_texture(
                gltf_material.normalTexture.index,
                TextureType::Data,
                texture_loader
            );

            const auto& texture = model.textures[gltf_material.normalTexture.index];
            const auto& sampler = model.samplers[texture.sampler];

            material.normal_sampler = to_vk_sampler(sampler, backend);
        } else {
            material.normal_texture = backend.get_default_normalmap_handle();
            material.normal_sampler = backend.get_default_sampler();
        }

        if (gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.index != -1) {
            material.metallic_roughness_texture = get_texture(
                gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.index,
                TextureType::Data,
                texture_loader
            );

            const auto& texture = model.textures[gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.index];
            const auto& sampler = model.samplers[texture.sampler];

            material.metallic_roughness_sampler = to_vk_sampler(sampler, backend);
        } else {
            material.metallic_roughness_texture = backend.get_white_texture_handle();
            material.metallic_roughness_sampler = backend.get_default_sampler();
        }

        if (gltf_material.emissiveTexture.index != -1) {
            material.emission_texture = get_texture(
                gltf_material.emissiveTexture.index,
                TextureType::Data,
                texture_loader
            );

            const auto& texture = model.textures[gltf_material.emissiveTexture.index];
            const auto& sampler = model.samplers[texture.sampler];

            material.emission_sampler = to_vk_sampler(sampler, backend);
        } else {
            material.emission_texture = backend.get_white_texture_handle();
            material.emission_sampler = backend.get_default_sampler();
        }

        const auto material_handle = material_storage.add_material(std::move(material));
        gltf_material_to_material_handle.emplace_back(material_handle);

        gltf_idx++;
    }

    logger->info("Imported all materials");
}

void GltfModel::import_meshes(SceneRenderer& renderer) {
    ZoneScoped;

    auto& mesh_storage = renderer.get_mesh_storage();

    gltf_primitive_to_mesh_primitive.reserve(512);

    for (const auto& mesh : model.meshes) {
        // Copy the vertex and index data into the appropriate buffers
        // Interleave the vertex data, because it's easier for me to handle conceptually
        // Maybe eventually profile splitting out positions for separate use

        auto imported_primitives = std::vector<Mesh>{};
        imported_primitives.reserve(mesh.primitives.size());

        auto primitive_idx = 0u;
        for (const auto& primitive : mesh.primitives) {
            const auto vertices = read_vertex_data(primitive, model);
            const auto indices = read_index_data(primitive, model);

            const auto mesh_maybe = mesh_storage.add_mesh(vertices, indices);

            if (mesh_maybe) {
                imported_primitives.emplace_back(*mesh_maybe);
            } else {
                logger->error(
                    "Could not import mesh primitive {} in mesh {}", primitive_idx,
                    mesh.name.empty() ? "Unnamed mesh" : mesh.name
                );
            }

            primitive_idx++;
        }

        gltf_primitive_to_mesh_primitive.emplace_back(imported_primitives);
    }
}

void GltfModel::calculate_bounding_sphere_and_footprint() {
    auto min_extents = glm::vec3{0.f};
    auto max_extents = glm::vec3{0.f};

    traverse_nodes(
        [&](const tinygltf::Node& node, const glm::mat4& local_to_world) {
            if (node.mesh != -1) {
                const auto& mesh = model.meshes[node.mesh];
                for (const auto& primitive : mesh.primitives) {
                    const auto position_accessor_index = primitive.attributes.at("POSITION");
                    const auto& position_accessor = model.accessors[position_accessor_index];
                    // Position accessor. It's min and max values are the min and max bounding box of the primitive
                    // This probably breaks for animated meshes
                    // Better solution: Save the glTF model's bounding sphere and footprint radius into an extension in the glTF
                    // file

                    const auto primitive_min = glm::make_vec3(position_accessor.minValues.data());
                    const auto primitive_max = glm::make_vec3(position_accessor.maxValues.data());

                    const auto primitive_min_modelspace =
                        local_to_world * glm::vec4{primitive_min, 1.f};
                    const auto primitive_max_modelspace =
                        local_to_world * glm::vec4{primitive_max, 1.f};

                    min_extents = glm::min(min_extents, glm::vec3{primitive_min_modelspace});
                    max_extents = glm::max(max_extents, glm::vec3{primitive_max_modelspace});

                    logger->info(
                        "New min: ({}, {}, {}) new max: ({}, {}, {})",
                        min_extents.x,
                        min_extents.y,
                        min_extents.z,
                        max_extents.x,
                        max_extents.y,
                        max_extents.z
                    );

                    break;
                }
            }
        }
    );

    // Bounding sphere center and radius
    const auto bounding_sphere_center = (min_extents + max_extents) / 2.f;
    const auto bounding_sphere_radius = glm::max(
        length(min_extents - bounding_sphere_center),
        length(max_extents - bounding_sphere_center)
    );

    // Footprint radius (radius along xz plane)
    const auto footprint_radius = glm::max(
        length(
            glm::vec2{min_extents.x, min_extents.z} -
            glm::vec2{
                bounding_sphere_center.x,
                bounding_sphere_center.z
            }
        ),
        length(
            glm::vec2{max_extents.x, max_extents.z} -
            glm::vec2{
                bounding_sphere_center.x,
                bounding_sphere_center.z
            }
        )
    );
    bounding_sphere = glm::vec4{bounding_sphere_center, bounding_sphere_radius};

    logger->info(
        "Bounding sphere: Center=({}, {}, {}) radius={}",
        bounding_sphere.x,
        bounding_sphere.y,
        bounding_sphere.z,
        bounding_sphere.w
    );
    logger->info("Footprint radius: {}", footprint_radius);
}

TextureHandle GltfModel::get_texture(
    const int gltf_texture_index, const TextureType type,
    TextureLoader& texture_storage
) {
    if (!gltf_texture_to_texture_handle.contains(gltf_texture_index)) {
        import_single_texture(gltf_texture_index, type, texture_storage);
    }

    return gltf_texture_to_texture_handle[gltf_texture_index];
}

void GltfModel::import_single_texture(
    const int gltf_texture_index, const TextureType type,
    TextureLoader& texture_storage
) {
    const auto& gltf_texture = model.textures[gltf_texture_index];

    std::string uri;

    const auto& image = model.images[gltf_texture.source];

    if (!image.uri.empty()) {
        uri = decode_percent_encoding(std::string_view{image.uri});
    } else {
        throw std::runtime_error{"Image has no URI! Embedded images are not supported"};
    }

    logger->info("Loading texture {}", uri);


    // Try to load a kTX version of the texture
    const auto texture_filepath = filepath.parent_path() / uri;
    auto ktx_texture_filepath = texture_filepath;
    ktx_texture_filepath.replace_extension("ktx2");
    auto handle = texture_storage.load_texture(ktx_texture_filepath, type);
    if (!handle) {
        logger->info(
            "Could not find KTX texture {}, trying regular texture {}", ktx_texture_filepath.string(),
            texture_filepath.string()
        );
        handle = texture_storage.load_texture(texture_filepath, type);
    }

    if (handle) {
        gltf_texture_to_texture_handle.emplace(gltf_texture_index, *handle);
    } else {
        throw std::runtime_error{fmt::format("Could not load image with URI {}", uri)};
    }
}

VkSampler GltfModel::to_vk_sampler(const tinygltf::Sampler& sampler, RenderBackend& backend) {
    auto create_info = VkSamplerCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .maxLod = 16,
    };

    switch (sampler.minFilter) {
    case TINYGLTF_TEXTURE_FILTER_NEAREST:
        create_info.minFilter = VK_FILTER_NEAREST;
        break;

    case TINYGLTF_TEXTURE_FILTER_LINEAR:
        create_info.minFilter = VK_FILTER_LINEAR;
        break;

    case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
        create_info.minFilter = VK_FILTER_NEAREST;
        create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        break;

    case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
        create_info.minFilter = VK_FILTER_LINEAR;
        create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        break;

    case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
        create_info.minFilter = VK_FILTER_NEAREST;
        create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        break;

    case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
        create_info.minFilter = VK_FILTER_LINEAR;
        create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }

    switch (sampler.magFilter) {
    case TINYGLTF_TEXTURE_FILTER_NEAREST:
        create_info.magFilter = VK_FILTER_NEAREST;
        break;

    case TINYGLTF_TEXTURE_FILTER_LINEAR:
        create_info.magFilter = VK_FILTER_LINEAR;
        break;
    }

    switch (sampler.wrapS) {
    case TINYGLTF_TEXTURE_WRAP_REPEAT:
        create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        break;

    case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
        create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        break;

    case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
        create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        break;
    }

    switch (sampler.wrapT) {
    case TINYGLTF_TEXTURE_WRAP_REPEAT:
        create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        break;

    case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
        create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        break;

    case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
        create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        break;
    }

    if (create_info.mipmapMode == VK_SAMPLER_MIPMAP_MODE_LINEAR) {
        create_info.anisotropyEnable = VK_TRUE;
        create_info.maxAnisotropy = 8;
    }

    return backend.get_global_allocator().get_sampler(create_info);
}

std::vector<StandardVertex>
read_vertex_data(const tinygltf::Primitive& primitive, const tinygltf::Model& model) {
    // Get the first attribute's index_count. All the attributes must have the same number of elements, no need to get all their counts
    const auto positions_index = primitive.attributes.at("POSITION");
    const auto positions_accessor = model.accessors[positions_index];
    const auto num_vertices = positions_accessor.count;

    auto vertices = std::vector<StandardVertex>();
    vertices.resize(num_vertices);

    copy_vertex_data_to_vector(primitive.attributes, model, vertices.data());

    return vertices;
}

std::vector<uint32_t>
read_index_data(const tinygltf::Primitive& primitive, const tinygltf::Model& model) {
    const auto& index_accessor = model.accessors[primitive.indices];
    const auto num_indices = index_accessor.count;

    auto indices = std::vector<uint32_t>{};
    indices.resize(num_indices);

    auto* const index_write_ptr = indices.data();

    const auto& index_buffer_view = model.bufferViews[index_accessor.bufferView];
    const auto& index_buffer = model.buffers[index_buffer_view.buffer];
    const auto* index_ptr_u8 =
        index_buffer.data.data() + index_buffer_view.byteOffset + index_accessor.byteOffset;

    if (index_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        const auto* index_read_ptr = reinterpret_cast<const uint16_t*>(index_ptr_u8);

        for (auto i = 0u; i < index_accessor.count; i++) {
            index_write_ptr[i] = index_read_ptr[i];
        }
    } else {
        memcpy(index_write_ptr, index_ptr_u8, index_accessor.count * sizeof(uint32_t));
    }

    return indices;
}

void copy_vertex_data_to_vector(
    const std::map<std::string, int>& attributes,
    const tinygltf::Model& model,
    StandardVertex* vertices
) {
    ZoneScoped;

    for (const auto& [attribute_name, attribute_index] : attributes) {
        ZoneScopedN("Attribute read");
        const auto& attribute_accessor = model.accessors[attribute_index];
        const auto& attribute_buffer_view = model.bufferViews[attribute_accessor.bufferView];
        const auto& attribute_buffer = model.buffers[attribute_buffer_view.buffer];

        auto* read_ptr_u8 =
            attribute_buffer.data.data() + attribute_buffer_view.byteOffset +
            attribute_accessor.byteOffset;
        auto* write_ptr = vertices;

        if (attribute_name == "POSITION") {
            const auto* read_ptr_vec3 = reinterpret_cast<const glm::vec3*>(read_ptr_u8);

            for (auto i = 0u; i < attribute_accessor.count; i++) {
                auto position = *read_ptr_vec3;
                position.x *= -1; // RH to LH conversion
                write_ptr->position = position;

                write_ptr++;
                read_ptr_vec3++;
            }
        } else if (attribute_name == "NORMAL") {
            const auto* read_ptr_vec3 = reinterpret_cast<const glm::vec3*>(read_ptr_u8);

            for (auto i = 0u; i < attribute_accessor.count; i++) {
                write_ptr->normal = *read_ptr_vec3;

                write_ptr++;
                read_ptr_vec3++;
            }
        } else if (attribute_name == "TANGENT") {
            const auto* read_ptr_vec3 = reinterpret_cast<const glm::vec3*>(read_ptr_u8);

            for (auto i = 0u; i < attribute_accessor.count; i++) {
                write_ptr->tangent = *read_ptr_vec3;

                write_ptr++;
                read_ptr_vec3++;
            }
        } else if (attribute_name == "TEXCOORD_0") {
            const auto* read_ptr_vec2 = reinterpret_cast<const glm::vec2*>(read_ptr_u8);

            for (auto i = 0u; i < attribute_accessor.count; i++) {
                write_ptr->texcoord = *read_ptr_vec2;

                write_ptr++;
                read_ptr_vec2++;
            }
        } else if (attribute_name == "COLOR_0") {
            const auto* read_ptr_vec4 = reinterpret_cast<const glm::vec4*>(read_ptr_u8);

            for (auto i = 0u; i < attribute_accessor.count; i++) {
                write_ptr->color = glm::packUnorm4x8(*read_ptr_vec4);

                write_ptr++;
                read_ptr_vec4++;
            }
        }

        // TODO: Other texcoord channels
    }
}
