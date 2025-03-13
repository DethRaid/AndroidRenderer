#include "gltf_model.hpp"

#include <span>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <magic_enum.hpp>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/android_sink.h>
#include <tracy/Tracy.hpp>
#include <fastgltf_parser.hpp>

#include "core/box.hpp"
#include "core/issue_breakpoint.hpp"
#include "core/percent_encoding.hpp"
#include "core/visitor.hpp"
#include "render/basic_pbr_material.hpp"
#include "render/scene_renderer.hpp"
#include "render/render_scene.hpp"
#include "render/texture_loader.hpp"

static std::shared_ptr<spdlog::logger> logger;

static bool front_face_ccw = true;

static std::vector<StandardVertex>
read_vertex_data(const fastgltf::Primitive& primitive, const fastgltf::Asset& model);

static std::vector<uint32_t>
read_index_data(const fastgltf::Primitive& primitive, const fastgltf::Asset& model);

static Box read_mesh_bounds(const fastgltf::Primitive& primitive, const fastgltf::Asset& model);

static void copy_vertex_data_to_vector(
    const std::unordered_map<std::string, std::size_t>& attributes,
    const fastgltf::Asset& model,
    StandardVertex* vertices
);

glm::mat4 get_node_to_parent_matrix(const fastgltf::Node& node) {
    auto matrix = glm::mat4{1.f};

    std::visit(
        Visitor{
            [&](const fastgltf::Node::TransformMatrix& node_matrix) {
                matrix = glm::make_mat4(node_matrix.data());
            },
            [&](const fastgltf::Node::TRS& trs) {
                const auto translation = glm::make_vec3(trs.translation.data());
                const auto rotation = glm::quat{trs.rotation[3], trs.rotation[0], trs.rotation[1], trs.rotation[2]};
                const auto scale_factors = glm::make_vec3(trs.scale.data());

                matrix = glm::translate(matrix, translation);
                matrix *= glm::toMat4(rotation);
                matrix = glm::scale(matrix, scale_factors);
            }
        },
        node.transform
    );

    return matrix;
}

GltfModel::GltfModel(
    std::filesystem::path filepath_in,
    std::unique_ptr<fastgltf::Asset>&& model,
    SceneRenderer& renderer
)
    : filepath{std::move(filepath_in)}, model{std::move(model)} {
    if (logger == nullptr) {
        logger = SystemInterface::get().get_logger("GltfModel");
    }

    ZoneScoped;

    logger->info("Beginning load of model {}", filepath.string());

    import_resources_for_model(renderer);

    calculate_bounding_sphere_and_footprint();

    logger->info("Loaded model {}", filepath.string());
}

glm::vec4 GltfModel::get_bounding_sphere() const { return bounding_sphere; }

const fastgltf::Asset& GltfModel::get_gltf_data() const { return *model; }

void GltfModel::add_primitives(RenderScene& scene, RenderGraph& graph) {
    traverse_nodes(
        [&](const fastgltf::Node& node, const glm::mat4& node_to_world) {
            if (node.meshIndex) {
                const auto mesh_index = *node.meshIndex;
                const auto& mesh = model->meshes[mesh_index];
                auto node_primitives = std::vector<PooledObject<MeshPrimitive>>{};
                node_primitives.reserve(mesh.primitives.size());
                for (auto i = 0u; i < mesh.primitives.size(); i++) {
                    const auto gltf_primitive = mesh.primitives.at(i);
                    const auto& imported_mesh = gltf_primitive_to_mesh_primitive.at(mesh_index).at(i);
                    const auto& imported_material = gltf_material_to_material_handle.at(
                        *gltf_primitive.materialIndex
                    );

                    const auto& bounds = imported_mesh->bounds;
                    const auto radius = glm::max(
                        glm::max(bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y), bounds.max.z - bounds.min.z
                    );

                    auto handle = scene.add_primitive(
                        graph, {
                            .data = PrimitiveDataGPU{
                                .model = node_to_world,
                                .inverse_model = glm::inverse(node_to_world),
                                .bounds_min_and_radius = {bounds.min, radius},
                                .bounds_max = {bounds.max, 0.f},
                                .mesh_id = imported_mesh.index,
                            },
                            .mesh = imported_mesh,
                            .material = imported_material,
                        }
                    );

                    node_primitives.emplace_back(handle);
                }
                scene_primitives.insert(scene_primitives.end(), node_primitives.begin(), node_primitives.end());
            }
        }
    );
    logger->info("Added nodes to the render scene");
}

void GltfModel::add_to_scene(RenderScene& scene) {
    auto& backend = RenderBackend::get();
    auto graph = RenderGraph{backend};

    add_primitives(scene, graph);

    graph.finish();
    backend.execute_graph(std::move(graph));
}

void GltfModel::import_resources_for_model(SceneRenderer& renderer) {
    ZoneScoped;

    // Upload all buffers and textures to the GPU, maintaining a mapping from glTF resource identifier to resource
    // Traverse the glTF scene. For each node with a mesh, create a `PlacesMeshPrimitive` with the mesh -> world
    // transformation matrix already calculated
    // Create a mapping from glTF scene to the `PlacesMeshPrimitive` objects it owns, so we can unload the scene

    import_meshes(renderer);

    import_materials(
        renderer.get_material_storage(), renderer.get_texture_loader(),
        RenderBackend::get()
    );

    logger->info("Imported resources");
}

static const fastgltf::Sampler default_sampler{};

void
GltfModel::import_materials(MaterialStorage& material_storage, TextureLoader& texture_loader, RenderBackend& backend) {
    ZoneScoped;

    gltf_material_to_material_handle.clear();
    gltf_material_to_material_handle.reserve(model->materials.size());

    for (const auto& gltf_material : model->materials) {
        const auto material_name = !gltf_material.name.empty()
                                       ? gltf_material.name
                                       : "Unnamed material";
        logger->info("Importing material {}", material_name);

        // Naive implementation creates a separate material for each glTF material
        // A better implementation would have a few pipeline objects that can be shared - e.g. we'd save the
        // pipeline create info and descriptor set layout info, and copy it down as needed

        auto material = BasicPbrMaterial{};
        material.name = material_name;

        if (gltf_material.alphaMode == fastgltf::AlphaMode::Opaque) {
            material.transparency_mode = TransparencyMode::Solid;
        } else if (gltf_material.alphaMode == fastgltf::AlphaMode::Mask) {
            material.transparency_mode = TransparencyMode::Cutout;
        } else if (gltf_material.alphaMode == fastgltf::AlphaMode::Blend) {
            material.transparency_mode = TransparencyMode::Translucent;
        }

        material.double_sided = gltf_material.doubleSided;
        material.front_face_ccw = front_face_ccw;

        material.gpu_data.base_color_tint = glm::vec4(
            glm::make_vec4(gltf_material.pbrData->baseColorFactor.data())
        );
        material.gpu_data.metalness_factor = static_cast<float>(gltf_material.pbrData->metallicFactor);
        material.gpu_data.roughness_factor = static_cast<float>(gltf_material.pbrData->roughnessFactor);
        material.gpu_data.opacity_threshold = gltf_material.alphaCutoff;

        const auto emissive_factor = glm::make_vec3(gltf_material.emissiveFactor.data());
        material.gpu_data.emission_factor = glm::vec4(emissive_factor, 1.f);
        if (length(emissive_factor) > 0) {
            material.emissive = true;
        }

        if (gltf_material.pbrData->baseColorTexture) {
            material.base_color_texture = get_texture(
                gltf_material.pbrData->baseColorTexture->textureIndex,
                TextureType::Color, texture_loader
            );

            const auto& texture = model->textures[gltf_material.pbrData->baseColorTexture->textureIndex];
            const auto& sampler = texture.samplerIndex ? model->samplers[*texture.samplerIndex] : default_sampler;

            material.base_color_sampler = to_vk_sampler(sampler, backend);
        } else {
            material.base_color_texture = backend.get_white_texture_handle();
            material.base_color_sampler = backend.get_default_sampler();
        }

        if (gltf_material.normalTexture) {
            material.normal_texture = get_texture(
                gltf_material.normalTexture->textureIndex,
                TextureType::Data,
                texture_loader
            );

            const auto& texture = model->textures[gltf_material.normalTexture->textureIndex];
            const auto& sampler = texture.samplerIndex ? model->samplers[*texture.samplerIndex] : default_sampler;

            material.normal_sampler = to_vk_sampler(sampler, backend);
        } else {
            material.normal_texture = backend.get_default_normalmap_handle();
            material.normal_sampler = backend.get_default_sampler();
        }

        if (gltf_material.pbrData->metallicRoughnessTexture) {
            material.metallic_roughness_texture = get_texture(
                gltf_material.pbrData->metallicRoughnessTexture->textureIndex,
                TextureType::Data,
                texture_loader
            );

            const auto& texture = model->textures[gltf_material.pbrData->metallicRoughnessTexture->textureIndex];
            const auto& sampler = texture.samplerIndex ? model->samplers[*texture.samplerIndex] : default_sampler;

            material.metallic_roughness_sampler = to_vk_sampler(sampler, backend);
        } else {
            material.metallic_roughness_texture = backend.get_white_texture_handle();
            material.metallic_roughness_sampler = backend.get_default_sampler();
        }

        if (gltf_material.emissiveTexture) {
            material.emission_texture = get_texture(
                gltf_material.emissiveTexture->textureIndex,
                TextureType::Data,
                texture_loader
            );

            const auto& texture = model->textures[gltf_material.emissiveTexture->textureIndex];
            const auto& sampler = texture.samplerIndex ? model->samplers[*texture.samplerIndex] : default_sampler;

            material.emission_sampler = to_vk_sampler(sampler, backend);

            material.emissive = true;
        } else {
            material.emission_texture = backend.get_white_texture_handle();
            material.emission_sampler = backend.get_default_sampler();
        }

        const auto material_handle = material_storage.add_material_instance(std::move(material));
        gltf_material_to_material_handle.emplace_back(material_handle);
    }

    logger->info("Imported all materials");
}

void GltfModel::import_meshes(SceneRenderer& renderer) {
    ZoneScoped;

    auto& mesh_storage = renderer.get_mesh_storage();

    gltf_primitive_to_mesh_primitive.reserve(512);

    for (const auto& mesh : model->meshes) {
        // Copy the vertex and index data into the appropriate buffers
        // Interleave the vertex data, because it's easier for me to handle conceptually
        // Maybe eventually profile splitting out positions for separate use

        auto imported_primitives = std::vector<MeshHandle>{};
        imported_primitives.reserve(mesh.primitives.size());

        auto primitive_idx = 0u;
        for (const auto& primitive : mesh.primitives) {
            const auto vertices = read_vertex_data(primitive, *model);
            const auto indices = read_index_data(primitive, *model);

            const auto mesh_bounds = read_mesh_bounds(primitive, *model);

            const auto mesh_maybe = mesh_storage.add_mesh(vertices, indices, mesh_bounds);

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
        [&](const fastgltf::Node& node, const glm::mat4& local_to_world) {
            if (node.meshIndex) {
                const auto& mesh = model->meshes[*node.meshIndex];
                for (const auto& primitive : mesh.primitives) {
                    const auto position_accessor_index = primitive.attributes.at("POSITION");
                    const auto& position_accessor = model->accessors[position_accessor_index];
                    // Position accessor. It's min and max values are the min and max bounding box of the primitive
                    // This probably breaks for animated meshes
                    // Better solution: Save the glTF model's bounding sphere and footprint radius into an extension in the glTF
                    // file

                    const auto primitive_min = glm::make_vec3(
                        std::get_if<std::vector<double>>(&position_accessor.min)->data()
                    );
                    const auto primitive_max = glm::make_vec3(
                        std::get_if<std::vector<double>>(&position_accessor.max)->data()
                    );

                    const auto primitive_min_modelspace = local_to_world * glm::vec4{primitive_min, 1.f};
                    const auto primitive_max_modelspace = local_to_world * glm::vec4{primitive_max, 1.f};

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
    const size_t gltf_texture_index, const TextureType type,
    TextureLoader& texture_storage
) {
    if (!gltf_texture_to_texture_handle.contains(gltf_texture_index)) {
        import_single_texture(gltf_texture_index, type, texture_storage);
    }

    return gltf_texture_to_texture_handle[gltf_texture_index];
}

void GltfModel::import_single_texture(
    const size_t gltf_texture_index, const TextureType type,
    TextureLoader& texture_storage
) {
    ZoneScoped;

    const auto& gltf_texture = model->textures[gltf_texture_index];
    const auto& image = model->images[*gltf_texture.imageIndex];

    auto image_data = std::vector<uint8_t>{};
    auto image_name = std::filesystem::path{image.name};
    auto mime_type = fastgltf::MimeType::None;

    std::visit(
        Visitor{
            [&](const auto&) {
                /* I'm just here so I don't get a compiler error */
            },
            [&](const fastgltf::sources::BufferView& buffer_view) {
                const auto& real_buffer_view = model->bufferViews[buffer_view.bufferViewIndex];
                const auto& buffer = model->buffers[real_buffer_view.bufferIndex];
                const auto* buffer_vector = std::get_if<fastgltf::sources::Vector>(&buffer.data);
                auto* data_pointer = buffer_vector->bytes.data();
                data_pointer += real_buffer_view.byteOffset;
                image_data = {data_pointer, data_pointer + real_buffer_view.byteLength};
                mime_type = buffer_view.mimeType;
            },
            [&](const fastgltf::sources::URI& file_path) {
                const auto uri = decode_percent_encoding(std::string_view{file_path.uri.raw()});

                logger->info("Loading texture {}", uri);

                // Try to load a KTX version of the texture
                const auto texture_filepath = filepath.parent_path() / std::filesystem::path{uri};
                auto ktx_texture_filepath = texture_filepath;
                ktx_texture_filepath.replace_extension("ktx2");
                auto data_maybe = SystemInterface::get().load_file(ktx_texture_filepath);
                if (data_maybe) {
                    image_data = std::move(*data_maybe);
                    image_name = ktx_texture_filepath;
                    mime_type = fastgltf::MimeType::KTX2;
                    return;
                }

                data_maybe = SystemInterface::get().load_file(texture_filepath);
                if (data_maybe) {
                    image_data = std::move(*data_maybe);
                    image_name = texture_filepath;
                    const auto& extension = texture_filepath.extension();
                    if (extension == ".png") {
                        mime_type = fastgltf::MimeType::PNG;
                    } else if (extension == ".jpg" || extension == ".jpeg") {
                        mime_type = fastgltf::MimeType::JPEG;
                    }
                    return;
                }

                throw std::runtime_error{fmt::format("Could not load image {}", texture_filepath.string())};
            },
            [&](const fastgltf::sources::Vector& vector_data) {
                image_data = {vector_data.bytes.begin(), vector_data.bytes.end()};
                mime_type = vector_data.mimeType;
            }
        },
        image.data
    );

    auto handle = tl::optional<TextureHandle>{};
    if (mime_type == fastgltf::MimeType::KTX2) {
        handle = texture_storage.upload_texture_ktx(image_name, image_data);
    } else if (mime_type == fastgltf::MimeType::PNG || mime_type == fastgltf::MimeType::JPEG) {
        handle = texture_storage.upload_texture_stbi(image_name, image_data, type);
    }

    if (handle) {
        gltf_texture_to_texture_handle.emplace(gltf_texture_index, *handle);
    } else {
        throw std::runtime_error{fmt::format("Could not load image {}", image_name.string())};
    }
}

VkSampler GltfModel::to_vk_sampler(const fastgltf::Sampler& sampler, RenderBackend& backend) {
    auto create_info = VkSamplerCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .maxLod = VK_LOD_CLAMP_NONE,
    };

    if (sampler.minFilter) {
        switch (*sampler.minFilter) {
        case fastgltf::Filter::Nearest:
            create_info.minFilter = VK_FILTER_NEAREST;
            create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;

        case fastgltf::Filter::Linear:
            create_info.minFilter = VK_FILTER_LINEAR;
            break;

        case fastgltf::Filter::NearestMipMapNearest:
            create_info.minFilter = VK_FILTER_NEAREST;
            create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;

        case fastgltf::Filter::LinearMipMapNearest:
            create_info.minFilter = VK_FILTER_LINEAR;
            create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;

        case fastgltf::Filter::NearestMipMapLinear:
            create_info.minFilter = VK_FILTER_NEAREST;
            create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;

        case fastgltf::Filter::LinearMipMapLinear:
            create_info.minFilter = VK_FILTER_LINEAR;
            create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        }
    }

    if (sampler.magFilter) {
        switch (*sampler.magFilter) {
        case fastgltf::Filter::Nearest:
            create_info.magFilter = VK_FILTER_NEAREST;
            break;

        case fastgltf::Filter::Linear:
            create_info.magFilter = VK_FILTER_LINEAR;
            break;

        default:
            logger->error("Invalid texture mag filter");
        }
    }

    switch (sampler.wrapS) {
    case fastgltf::Wrap::Repeat:
        create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        break;

    case fastgltf::Wrap::ClampToEdge:
        create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        break;

    case fastgltf::Wrap::MirroredRepeat:
        create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        break;
    }

    switch (sampler.wrapT) {
    case fastgltf::Wrap::Repeat:
        create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        break;

    case fastgltf::Wrap::ClampToEdge:
        create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        break;

    case fastgltf::Wrap::MirroredRepeat:
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
read_vertex_data(const fastgltf::Primitive& primitive, const fastgltf::Asset& model) {
    // Get the first attribute's index_count. All the attributes must have the same number of elements, no need to get all their counts
    const auto positions_index = primitive.attributes.at("POSITION");
    const auto positions_accessor = model.accessors[positions_index];
    const auto num_vertices = positions_accessor.count;

    auto vertices = std::vector<StandardVertex>();
    vertices.resize(
        num_vertices, StandardVertex{
            .position = glm::vec3{},
            .normal = glm::vec3{0, 0, 1},
            .tangent = glm::vec3{1, 0, 0},
            .texcoord = {},
            .color = glm::packUnorm4x8(glm::vec4{1, 1, 1, 1}),
        }
    );

    copy_vertex_data_to_vector(primitive.attributes, model, vertices.data());

    return vertices;
}

std::vector<uint32_t>
read_index_data(const fastgltf::Primitive& primitive, const fastgltf::Asset& model) {
    const auto& index_accessor = model.accessors[*primitive.indicesAccessor];
    const auto num_indices = index_accessor.count;

    auto indices = std::vector<uint32_t>{};
    indices.resize(num_indices);

    auto* const index_write_ptr = indices.data();

    const auto& index_buffer_view = model.bufferViews[*index_accessor.bufferViewIndex];
    const auto& index_buffer = model.buffers[index_buffer_view.bufferIndex];
    const auto* index_buffer_data = std::get_if<fastgltf::sources::Vector>(&index_buffer.data);
    const auto* index_ptr_u8 = index_buffer_data->bytes.data() + index_buffer_view.byteOffset + index_accessor.
        byteOffset;

    if (index_accessor.componentType == fastgltf::ComponentType::UnsignedShort) {
        const auto* index_read_ptr = reinterpret_cast<const uint16_t*>(index_ptr_u8);

        for (auto i = 0u; i < index_accessor.count; i++) {
            index_write_ptr[i] = index_read_ptr[i];
        }
    } else {
        memcpy(index_write_ptr, index_ptr_u8, index_accessor.count * sizeof(uint32_t));
    }

    return indices;
}

Box read_mesh_bounds(const fastgltf::Primitive& primitive, const fastgltf::Asset& model) {
    const auto position_attribute_idx = primitive.attributes.at("POSITION");
    const auto& position_accessor = model.accessors[position_attribute_idx];
    const auto* min = std::get_if<std::vector<double>>(&position_accessor.min);
    const auto* max = std::get_if<std::vector<double>>(&position_accessor.max);

    return {.min = glm::make_vec3(min->data()), .max = glm::make_vec3(max->data())};
}

void copy_vertex_data_to_vector(
    const std::unordered_map<std::string, std::size_t>& attributes,
    const fastgltf::Asset& model,
    StandardVertex* vertices
) {
    ZoneScoped;

    for (const auto& [attribute_name, attribute_index] : attributes) {
        ZoneScopedN("Attribute read");
        const auto& attribute_accessor = model.accessors[attribute_index];
        const auto& attribute_buffer_view = model.bufferViews[*attribute_accessor.bufferViewIndex];
        const auto& attribute_buffer = model.buffers[attribute_buffer_view.bufferIndex];
        const auto* attribute_data = std::get_if<fastgltf::sources::Vector>(&attribute_buffer.data);

        auto* read_ptr_u8 = attribute_data->bytes.data() + attribute_buffer_view.byteOffset + attribute_accessor.
            byteOffset;
        auto* write_ptr = vertices;

        write_ptr->color = glm::packUnorm4x8(glm::vec4{1.f});

        if (attribute_name == "POSITION") {
            const auto* read_ptr_vec3 = reinterpret_cast<const glm::vec3*>(read_ptr_u8);

            for (auto i = 0u; i < attribute_accessor.count; i++) {
                //position.x *= -1;
                write_ptr->position = *read_ptr_vec3;

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
            const auto* read_ptr_vec4 = reinterpret_cast<const glm::vec4*>(read_ptr_u8);

            // Massive hack! This code assumes that all primitives in a model will have the same handedness
            // This is not guraunteed by the glTF spec. The only thing it seems to garuantee is that all vertices in a
            // given triangle will have the same handedness
            if (read_ptr_vec4->w < 0) {
                front_face_ccw = false;
            }

            for (auto i = 0u; i < attribute_accessor.count; i++) {
                write_ptr->tangent = *read_ptr_vec4;

                write_ptr++;
                read_ptr_vec4++;
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
