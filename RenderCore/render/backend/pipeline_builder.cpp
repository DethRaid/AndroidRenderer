#include "pipeline_builder.hpp"

#include <magic_enum.hpp>
#include <span>
#include <spirv_reflect.h>
#include <tracy/Tracy.hpp>

#include "pipeline_cache.hpp"
#include "core/system_interface.hpp"
#include "shared/vertex_data.hpp"

static std::shared_ptr<spdlog::logger> logger;

constexpr uint32_t variable_size_array_max_size = 65536;

constexpr const auto vertex_position_input_binding = VkVertexInputBindingDescription{
    .binding = 0,
    .stride = sizeof(VertexPosition),
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
};

constexpr const auto vertex_data_input_binding = VkVertexInputBindingDescription{
    .binding = 1,
    .stride = sizeof(StandardVertexData),
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX

};

// Positions
constexpr const auto vertex_position_attribute = VkVertexInputAttributeDescription{
    .location = 0,
    .binding = 0,
    .format = VK_FORMAT_R32G32B32_SFLOAT,
    .offset = 0,
};

// Normals
constexpr const auto vertex_normal_attribute = VkVertexInputAttributeDescription{
    .location = 1,
    .binding = 1,
    .format = VK_FORMAT_R32G32B32_SFLOAT,
    .offset = offsetof(StandardVertexData, normal),
};

// Tangents
constexpr const auto vertex_tangent_attribute = VkVertexInputAttributeDescription{
    .location = 2,
    .binding = 1,
    .format = VK_FORMAT_R32G32B32_SFLOAT,
    .offset = offsetof(StandardVertexData, tangent),
};

// Texcoord
constexpr const auto vertex_texcoord_attribute = VkVertexInputAttributeDescription{
    .location = 3,
    .binding = 1,
    .format = VK_FORMAT_R32G32_SFLOAT,
    .offset = offsetof(StandardVertexData, texcoord),
};

// Color
constexpr const auto vertex_color_attribute = VkVertexInputAttributeDescription{
    .location = 4,
    .binding = 1,
    .format = VK_FORMAT_R8G8B8A8_UNORM,
    .offset = offsetof(StandardVertexData, color),
};

VkDescriptorType to_vk_type(SpvReflectDescriptorType type);

/**
 * Collects the descriptor sets from the provided list of descriptor sets. Performs basic validation that the sets
 * match the sets that have already been collected
 *
 * @param shader_path Path to the shader that the descriptor sets came from. Used for logging
 * @param sets The descriptor sets to collect
 * @param shader_stage The shader stage these descriptor sets came from
 * @return True if there was an error, false if everything's fine
 */
bool collect_descriptor_sets(
    const std::filesystem::path& shader_path,
    const std::vector<SpvReflectDescriptorSet*>& sets,
    VkShaderStageFlagBits shader_stage,
    std::unordered_map<uint32_t, DescriptorSetInfo>& descriptor_sets
);

bool collect_push_constants(
    const std::filesystem::path& shader_path,
    const std::vector<SpvReflectBlockVariable*>& spv_push_constants,
    VkShaderStageFlagBits shader_stage,
    std::vector<VkPushConstantRange>& push_constants
);

bool collect_vertex_attributes(
    const std::filesystem::path& shader_path,
    const std::vector<SpvReflectInterfaceVariable*>& inputs,
    std::vector<VkVertexInputBindingDescription>& vertex_inputs,
    std::vector<VkVertexInputAttributeDescription>& vertex_attributes
);

GraphicsPipelineBuilder::GraphicsPipelineBuilder(PipelineCache& cache_in) : cache{ cache_in } {
    if (logger == nullptr) {
        logger = SystemInterface::get().get_logger("GraphicsPipelineBuilder");
        logger->set_level(spdlog::level::warn);
    }

    set_depth_state({});
    set_raster_state({});
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::set_name(std::string_view name_in) {
    name = name_in;

    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::set_topology(VkPrimitiveTopology topology_in) {
    topology = topology_in;

    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::set_vertex_shader(const std::filesystem::path& vertex_path) {
    if (vertex_shader) {
        throw std::runtime_error{ "Vertex shader already loaded set" };
    }
    const auto vertex_shader_maybe = SystemInterface::get().load_file(vertex_path);

    if (!vertex_shader_maybe) {
        throw std::runtime_error{ "Could not load vertex shader" };
    }

    vertex_shader = *vertex_shader_maybe;
    vertex_shader_name = vertex_path.string();

    const auto shader_module = spv_reflect::ShaderModule{
        *vertex_shader,
        SPV_REFLECT_MODULE_FLAG_NO_COPY
    };
    if (shader_module.GetResult() != SpvReflectResult::SPV_REFLECT_RESULT_SUCCESS) {
        throw std::runtime_error{ "Could not perform reflection on vertex shader" };
    }

    bool has_error = false;

    logger->debug("Beginning reflection on vertex shader {}", vertex_shader_name);

    // Collect descriptor set info
    uint32_t set_count;
    auto result = shader_module.EnumerateDescriptorSets(&set_count, nullptr);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);
    auto sets = std::vector<SpvReflectDescriptorSet*>{ set_count };
    result = shader_module.EnumerateDescriptorSets(&set_count, sets.data());
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    has_error |= collect_descriptor_sets(
        vertex_path, sets, VK_SHADER_STAGE_VERTEX_BIT,
        descriptor_sets
    );

    // Collect push constant info
    uint32_t constant_count;
    result = shader_module.EnumeratePushConstantBlocks(&constant_count, nullptr);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);
    auto spv_push_constants = std::vector<SpvReflectBlockVariable*>{ constant_count };
    result = shader_module.EnumeratePushConstantBlocks(&constant_count, spv_push_constants.data());
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    has_error |= collect_push_constants(
        vertex_path, spv_push_constants, VK_SHADER_STAGE_VERTEX_BIT,
        push_constants
    );

    // Collect inputs
    uint32_t input_count;
    result = shader_module.EnumerateInputVariables(&input_count, nullptr);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);
    auto spv_vertex_inputs = std::vector<SpvReflectInterfaceVariable*>{ input_count };
    result = shader_module.EnumerateInputVariables(&input_count, spv_vertex_inputs.data());
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    vertex_inputs.clear();
    vertex_inputs.reserve(spv_vertex_inputs.size());
    vertex_attributes.clear();
    vertex_attributes.reserve(spv_vertex_inputs.size());
    has_error |= collect_vertex_attributes(
        vertex_path, spv_vertex_inputs, vertex_inputs,
        vertex_attributes
    );

    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::set_geometry_shader(const std::filesystem::path& geometry_path) {
    if (geometry_shader) {
        throw std::runtime_error{ "Geometry shader already set!" };
    }

    const auto geometry_shader_maybe = SystemInterface::get().load_file(geometry_path);
    if (!geometry_shader_maybe) {
        throw std::runtime_error{ "Could not load geometry shader" };
    }

    geometry_shader = *geometry_shader_maybe;
    geometry_shader_name = geometry_path.string();

    const auto shader_module = spv_reflect::ShaderModule{
        *geometry_shader,
        SPV_REFLECT_MODULE_FLAG_NO_COPY
    };
    if (shader_module.GetResult() != SpvReflectResult::SPV_REFLECT_RESULT_SUCCESS) {
        throw std::runtime_error{ "Could not perform reflection on geometry shader" };
    }

    bool has_error = false;

    logger->debug("Beginning reflection on geometry shader {}", geometry_shader_name);

    // Collect descriptor set info
    uint32_t set_count;
    auto result = shader_module.EnumerateDescriptorSets(&set_count, nullptr);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);
    auto sets = std::vector<SpvReflectDescriptorSet*>{ set_count };
    result = shader_module.EnumerateDescriptorSets(&set_count, sets.data());
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    has_error |= collect_descriptor_sets(
        geometry_path, sets, VK_SHADER_STAGE_GEOMETRY_BIT,
        descriptor_sets
    );

    // Collect push constant info
    uint32_t constant_count;
    result = shader_module.EnumeratePushConstantBlocks(&constant_count, nullptr);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);
    auto spv_push_constants = std::vector<SpvReflectBlockVariable*>{ constant_count };
    result = shader_module.EnumeratePushConstantBlocks(&constant_count, spv_push_constants.data());
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    has_error |= collect_push_constants(
        geometry_path, spv_push_constants, VK_SHADER_STAGE_GEOMETRY_BIT,
        push_constants
    );

    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::set_fragment_shader(const std::filesystem::path& fragment_path) {
    if (fragment_shader) {
        throw std::runtime_error{ "Fragment shader already set" };
    }

    const auto fragment_shader_maybe = SystemInterface::get().load_file(fragment_path);

    if (!fragment_shader_maybe) {
        throw std::runtime_error{ "Could not load fragment shader" };
    }

    fragment_shader = *fragment_shader_maybe;
    fragment_shader_name = fragment_path.string();

    logger->debug("Beginning reflection on fragment shader {}", fragment_shader_name);

    collect_bindings(
        *fragment_shader, fragment_path.string(), VK_SHADER_STAGE_FRAGMENT_BIT,
        descriptor_sets,
        push_constants
    );

    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::set_depth_state(const DepthStencilState& depth_stencil) {
    depth_stencil_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        // TODO: We might need flags
        .depthTestEnable = depth_stencil.enable_depth_test ? VK_TRUE : VK_FALSE,
        .depthWriteEnable = depth_stencil.enable_depth_write ? VK_TRUE : VK_FALSE,
        .depthCompareOp = depth_stencil.compare_op,
        .depthBoundsTestEnable = depth_stencil.enable_depth_bounds_test ? VK_TRUE : VK_FALSE,
        .stencilTestEnable = depth_stencil.enable_stencil_test ? VK_TRUE : VK_FALSE,
        .front = depth_stencil.front_face_stencil_state,
        .back = depth_stencil.back_face_stencil_state,
        .minDepthBounds = depth_stencil.min_depth_bounds,
        .maxDepthBounds = depth_stencil.max_depth_bounds,
    };

    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::set_raster_state(const RasterState& raster_state_in) {
    raster_state = VkPipelineRasterizationStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = raster_state_in.depth_clamp_enable,
        .polygonMode = raster_state_in.polygon_mode,
        .cullMode = raster_state_in.cull_mode,
        .frontFace = raster_state_in.front_face,
        .lineWidth = raster_state_in.line_width,
    };

    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::add_blend_flag(VkPipelineColorBlendStateCreateFlagBits flag) {
    blend_flags |= flag;

    return *this;
}

GraphicsPipelineBuilder&
GraphicsPipelineBuilder::set_blend_state(
    uint32_t color_target_index,
    const VkPipelineColorBlendAttachmentState& blend
) {
    if (blends.size() <= color_target_index) {
        blends.resize(color_target_index + 1);
    }

    blends[color_target_index] = blend;

    return *this;
}

GraphicsPipelineHandle GraphicsPipelineBuilder::build() const {
    return cache.create_pipeline(*this);
}

bool collect_descriptor_sets(
    const std::filesystem::path& shader_path,
    const std::vector<SpvReflectDescriptorSet*>& sets,
    const VkShaderStageFlagBits shader_stage,
    std::unordered_map<uint32_t, DescriptorSetInfo>& descriptor_sets
) {
    bool has_error = false;
    for (const auto* set : sets) {
        if (auto itr = descriptor_sets.find(set->set); itr != descriptor_sets.end()) {
            // We saw this set in the previous shader. Validate that it's the same as the previous, and mark it with
            // the new shader stage
            auto& set_info = itr->second;
            auto& known_bindings = set_info.bindings;
            for (auto* binding : std::span{ set->bindings, set->bindings + set->binding_count }) {
                const auto vk_type = to_vk_type(binding->descriptor_type);
                if (auto binding_itr = known_bindings.find(binding->binding); binding_itr !=
                    known_bindings.end()) {
                    // We saw this binding already. Verify that it's the same
                    auto& existing_binding = binding_itr->second;

                    if (existing_binding.descriptorCount != binding->count) {
                        logger->error(
                            "Descriptor set={} binding={} in shader {} has count {}, previous shader said it had count {}",
                            set->set, binding->binding, shader_path.string(), binding->count,
                            binding_itr->second.descriptorCount
                        );
                        has_error = true;
                    }

                    if (existing_binding.descriptorType != vk_type) {
                        logger->error(
                            "Descriptor set={} binding={} in shader {} has type {}, previous shader said it had type {}",
                            binding->set, binding->binding, shader_path.string(), vk_type,
                            existing_binding.descriptorType
                        );
                        has_error = true;
                    }

                    logger->trace(
                        "Appending shader stage {} to descriptor {}.{}",
                        magic_enum::enum_name(shader_stage),
                        set->set, binding->binding
                    );

                    existing_binding.stageFlags |= shader_stage;
                } else {
                    logger->trace(
                        "Adding new descriptor {}.{} with count {} to existing set for shader stage {}",
                        set->set,
                        binding->binding,
                        binding->count,
                        magic_enum::enum_name(shader_stage)
                    );

                    // This binding is new! Create it and add it
                    known_bindings.emplace(
                        binding->binding,
                        VkDescriptorSetLayoutBinding{
                            .binding = binding->binding,
                            .descriptorType = vk_type,
                            .descriptorCount = binding->count > 0 ? binding->count : variable_size_array_max_size,
                            .stageFlags = static_cast<VkShaderStageFlags>(shader_stage),
                            .pImmutableSamplers = nullptr
                        }
                    );

                    if (binding->count == 0) {
                        set_info.has_variable_count_binding = true;
                    }
                }
            }
        } else {
            // The set is new. Construct new bindings for it and add them all
            auto set_info = DescriptorSetInfo{};
            logger->trace("Adding new descriptor set {}", set->set);
            for (auto* binding : std::span{ set->bindings, set->bindings + set->binding_count }) {
                logger->trace(
                    "Adding new descriptor {}.{} with count {} for shader stage {}",
                    set->set,
                    binding->binding,
                    binding->count,
                    magic_enum::enum_name(shader_stage)
                );
                set_info.bindings.emplace(
                    binding->binding,
                    VkDescriptorSetLayoutBinding{
                        .binding = binding->binding,
                        .descriptorType = to_vk_type(binding->descriptor_type),
                        .descriptorCount = binding->count > 0 ? binding->count : variable_size_array_max_size,
                        .stageFlags = static_cast<VkShaderStageFlags>(shader_stage),
                        .pImmutableSamplers = nullptr
                    }
                );

                if (binding->count == 0) {
                    set_info.has_variable_count_binding = true;
                }
            }

            descriptor_sets.emplace(set->set, set_info);
        }
    }

    return has_error;
}

bool collect_push_constants(
    const std::filesystem::path& shader_path,
    const std::vector<SpvReflectBlockVariable*>& spv_push_constants,
    VkShaderStageFlagBits shader_stage,
    std::vector<VkPushConstantRange>& push_constants
) {
    bool has_error = false;

    for (const auto& constant_range : spv_push_constants) {
        auto existing_constant = std::find_if(
            push_constants.begin(), push_constants.end(),
            [&](const VkPushConstantRange& existing_range) {
                return existing_range.offset ==
                    constant_range->offset;
            }
        );
        if (existing_constant != push_constants.end()) {
            if (existing_constant->size != constant_range->size) {
                logger->error(
                    "Push constant range at offset {} has size {} in shader {}, but it had size {} earlier",
                    constant_range->offset, constant_range->size, shader_path.string(),
                    existing_constant->size
                );
                has_error = true;

                // Expand the size - is this correct?
                existing_constant->size = std::max(existing_constant->size, constant_range->size);
            }

            // Make the range visible to this stage
            existing_constant->stageFlags |= shader_stage;
        } else {
            // New range!
            push_constants.emplace_back(
                VkPushConstantRange{
                    .stageFlags = static_cast<VkShaderStageFlags>(shader_stage),
                    .offset = constant_range->offset,
                    .size = constant_range->size
                }
            );
        }
    }

    return has_error;
}

bool collect_bindings(
    const std::vector<uint8_t>& shader_instructions, const std::string& shader_name,
    VkShaderStageFlagBits shader_stage,
    std::unordered_map<uint32_t, DescriptorSetInfo>& descriptor_sets,
    std::vector<VkPushConstantRange>& push_constants
) {
    if (logger == nullptr) {
        logger = SystemInterface::get().get_logger("GraphicsPipelineBuilder");
        logger->set_level(spdlog::level::warn);
    }

    const auto shader_module = spv_reflect::ShaderModule{
        shader_instructions,
        SPV_REFLECT_MODULE_FLAG_NO_COPY
    };
    if (shader_module.GetResult() != SpvReflectResult::SPV_REFLECT_RESULT_SUCCESS) {
        throw std::runtime_error{ "Could not perform reflection on shader" };
    }

    bool has_error = false;

    // Collect descriptor set info
    uint32_t set_count;
    auto result = shader_module.EnumerateDescriptorSets(&set_count, nullptr);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);
    auto sets = std::vector<SpvReflectDescriptorSet*>{ set_count };
    result = shader_module.EnumerateDescriptorSets(&set_count, sets.data());
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    has_error |= collect_descriptor_sets(shader_name, sets, shader_stage, descriptor_sets);

    // Collect push constant info
    uint32_t constant_count;
    result = shader_module.EnumeratePushConstantBlocks(&constant_count, nullptr);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);
    auto spv_push_constants = std::vector<SpvReflectBlockVariable*>{ constant_count };
    result = shader_module.EnumeratePushConstantBlocks(&constant_count, spv_push_constants.data());
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    has_error |= collect_push_constants(
        shader_name, spv_push_constants, shader_stage,
        push_constants
    );

    return has_error;
}

bool collect_vertex_attributes(
    const std::filesystem::path& shader_path,
    const std::vector<SpvReflectInterfaceVariable*>& inputs,
    std::vector<VkVertexInputBindingDescription>& vertex_inputs,
    std::vector<VkVertexInputAttributeDescription>& vertex_attributes
) {
    bool has_error = false;

    for (const auto* input : inputs) {
        if (input->location == 0) {
            // Vertex?
            if (input->format != SPV_REFLECT_FORMAT_R32G32B32_SFLOAT) {
                logger->error(
                    "Vertex input at location 0 should be position, but it's in the wrong format"
                );
                has_error = true;
            } else {
                vertex_inputs.emplace_back(vertex_position_input_binding);
                vertex_attributes.emplace_back(vertex_position_attribute);
            }
        } else if (input->location == 1) {
            // Normal?
            if (input->format != SPV_REFLECT_FORMAT_R32G32B32_SFLOAT) {
                logger->error(
                    "Vertex input at location 1 should be normals, but it's in the wrong format"
                );
                has_error = true;
            } else {
                if (std::find_if(
                    vertex_inputs.begin(), vertex_inputs.end(),
                    [&](const VkVertexInputBindingDescription& input_binding) {
                        return input_binding.binding ==
                            vertex_data_input_binding.binding;
                    }
                ) == vertex_inputs.cend()) {
                    vertex_inputs.emplace_back(vertex_data_input_binding);
                }

                vertex_attributes.emplace_back(vertex_normal_attribute);
            }
        } else if (input->location == 2) {
            // Tangent?
            if (input->format != SPV_REFLECT_FORMAT_R32G32B32_SFLOAT) {
                logger->error(
                    "Vertex input at location 2 should be tangents, but it's in the wrong format"
                );
                has_error = true;
            } else {
                if (std::find_if(
                    vertex_inputs.begin(), vertex_inputs.end(),
                    [&](const VkVertexInputBindingDescription& input_binding) {
                        return input_binding.binding ==
                            vertex_data_input_binding.binding;
                    }
                ) == vertex_inputs.cend()) {
                    vertex_inputs.emplace_back(vertex_data_input_binding);
                }

                vertex_attributes.emplace_back(vertex_tangent_attribute);
            }
        } else if (input->location == 3) {
            // Texcoord?
            if (input->format != SPV_REFLECT_FORMAT_R32G32_SFLOAT) {
                logger->error(
                    "Vertex input at location 3 should be texcoords, but it's in the wrong format"
                );
                has_error = true;
            } else {
                if (std::find_if(
                    vertex_inputs.begin(), vertex_inputs.end(),
                    [&](const VkVertexInputBindingDescription& input_binding) {
                        return input_binding.binding ==
                            vertex_data_input_binding.binding;
                    }
                ) == vertex_inputs.cend()) {
                    vertex_inputs.emplace_back(vertex_data_input_binding);
                }

                vertex_attributes.emplace_back(vertex_texcoord_attribute);
            }
        } else if (input->location == 4) {
            // Texcoord?
            if (input->format != SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT) {
                logger->error(
                    "Vertex input at location 4 should be colors, but it's in the wrong format"
                );
                has_error = true;
            } else {
                if (std::find_if(
                    vertex_inputs.begin(), vertex_inputs.end(),
                    [&](const VkVertexInputBindingDescription& input_binding) {
                        return input_binding.binding ==
                            vertex_data_input_binding.binding;
                    }
                ) == vertex_inputs.cend()) {
                    vertex_inputs.emplace_back(vertex_data_input_binding);
                }

                vertex_attributes.emplace_back(vertex_color_attribute);
            }
        } else if (input->location != -1) {
            // -1 is used for some builtin things i guess
            // I can't
            logger->error("Vertex input {} unrecognized", input->location);
            has_error = true;
        }
    }

    return has_error;
}