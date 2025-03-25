#pragma once

#include <filesystem>
#include <EASTL/vector.h>
#include <cstdint>
#include <EASTL/unordered_map.h>
#include <EASTL/string.h>

#include <volk.h>

#include "render/backend/graphics_pipeline.hpp"
#include "render/backend/handles.hpp"

class PipelineCache;

class RenderBackend;

struct SpvReflectDescriptorSet;
struct SpvReflectBlockVariable;
struct SpvReflectInterfaceVariable;

bool collect_bindings(
    const eastl::vector<std::byte>& shader_instructions,
    std::string_view shader_name,
    VkShaderStageFlags shader_stage,
    eastl::vector<DescriptorSetInfo>& descriptor_sets,
    eastl::vector<VkPushConstantRange>& push_constants
);

struct VertexLayout {
    eastl::vector<VkVertexInputBindingDescription> input_bindings;
    eastl::unordered_map<eastl::string, VkVertexInputAttributeDescription> attributes;
};

/**
 * Depth/stencil state struct with sane defaults
 *
 * Enable depth test and depth writes
 *
 * Set compare op to greater, because we use a reversed-z depth buffer
 *
 * Disable stencil test
 */
struct DepthStencilState {
    bool enable_depth_test = true;

    bool enable_depth_write = true;

    VkCompareOp compare_op = VK_COMPARE_OP_GREATER;

    bool enable_depth_bounds_test = false;

    bool enable_stencil_test = false;

    VkStencilOpState front_face_stencil_state = {};

    VkStencilOpState back_face_stencil_state = {};

    float min_depth_bounds = 0.f;

    float max_depth_bounds = 0.f;
};

struct RasterState {
    VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;

    float line_width = 1.f;

    bool depth_clamp_enable = false;
};

class GraphicsPipelineBuilder {
    friend class PipelineCache;

public:
    explicit GraphicsPipelineBuilder(PipelineCache& cache_in);

    GraphicsPipelineBuilder& set_name(std::string_view name_in);

    GraphicsPipelineBuilder& set_vertex_layout(VertexLayout& layout);

    GraphicsPipelineBuilder& use_standard_vertex_layout();

    GraphicsPipelineBuilder& use_imgui_vertex_layout();

    GraphicsPipelineBuilder& set_topology(VkPrimitiveTopology topology_in);

    /**
     * Sets the vertex shader to use
     *
     * This method loads the vertex shader from storage, performs reflection on it to see what descriptors it needs, and
     * saves that information internally
     *
     * Vertex shader inputs must follow a specific format:
     * 0: position (vec3)
     * 1: texcoord (vec2)
     * 2: normals (vec3)
     * 3: tangents (vec3)
     *
     * Maybe in the future I'll have cool vertex factories, for the present it's fine
     *
     * The vertex shader must already be compiled to SPIR-V
     *
     * Calling this method multiple times is a problem
     */
    GraphicsPipelineBuilder& set_vertex_shader(const std::filesystem::path& vertex_path);

    GraphicsPipelineBuilder& set_geometry_shader(const std::filesystem::path& geometry_path);

    GraphicsPipelineBuilder& set_fragment_shader(const std::filesystem::path& fragment_path);

    GraphicsPipelineBuilder& set_depth_state(const DepthStencilState& depth_stencil);

    GraphicsPipelineBuilder& set_raster_state(const RasterState& raster_state_in);

    GraphicsPipelineBuilder& add_blend_flag(VkPipelineColorBlendStateCreateFlagBits flag);

    GraphicsPipelineBuilder& set_blend_state(
        uint32_t color_target_index, const VkPipelineColorBlendAttachmentState& blend
    );

    /**
     * Enables using the pipeline in a pipeline group
     */
    GraphicsPipelineBuilder& enable_dgc();

    GraphicsPipelineHandle build();

private:
    PipelineCache& cache;

    std::string name;

    /**
     * Vertex shader SPIR-V code. If this is present, you may not load another vertex shader
     */
    std::optional<eastl::vector<std::byte>> vertex_shader;

    std::string vertex_shader_name;

    std::optional<eastl::vector<std::byte>> geometry_shader;

    std::string geometry_shader_name;

    std::optional<eastl::vector<std::byte>> fragment_shader;

    std::string fragment_shader_name;

    /**
     * Map from set number to the descriptor set info
     *
     * Map so that the vertex and fragment shaders need not have contiguous descriptor sets
     *
     * However, each set in the vertex and fragment shader must be the same - vertex shader set 0 must be the same as
     * fragment shader set 0
     */
    eastl::vector<DescriptorSetInfo> descriptor_sets;

    eastl::vector<VkPushConstantRange> push_constants;

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {};

    VkPipelineRasterizationStateCreateInfo raster_state = {};

    VkPipelineColorBlendStateCreateFlags blend_flags = {};
    eastl::vector<VkPipelineColorBlendAttachmentState> blends = {};

    bool need_position_buffer = false;
    bool need_data_buffer = false;
    bool need_primitive_id_buffer = false;
    eastl::vector<VkVertexInputBindingDescription> vertex_inputs;
    eastl::vector<VkVertexInputAttributeDescription> vertex_attributes;

    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VertexLayout* vertex_layout;

    bool should_enable_dgc;
};
