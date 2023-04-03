#pragma once

#include <filesystem>
#include <vector>
#include <cstdint>
#include <unordered_map>

#include <volk.h>
#include <tl/optional.hpp>

class CommandBuffer;

class Pipeline;

class RenderBackend;

struct SpvReflectDescriptorSet;
struct SpvReflectBlockVariable;
struct SpvReflectInterfaceVariable;

struct DescriptorSetInfo {
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;

    bool has_variable_count_binding = false;
};

bool collect_bindings(const std::vector<uint8_t>& shader_instructions, const std::string& shader_name,
                      VkShaderStageFlagBits shader_stage, std::unordered_map<uint32_t, DescriptorSetInfo>&
                              descriptor_sets, std::vector<VkPushConstantRange>& push_constants);

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

    VkCompareOp compare_op = VK_COMPARE_OP_LESS;

    bool enable_depth_bounds_test = false;

    bool enable_stencil_test = false;

    VkStencilOpState front_face_stencil_state = {};

    VkStencilOpState back_face_stencil_state = {};

    float min_depth_bounds = 0.f;

    float max_depth_bounds = 0.f;
};

struct RasterState {
    VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;

    VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT;

    VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    float line_width = 1.f;

    bool depth_clamp_enable = false;
};

class PipelineBuilder {
public:
    explicit PipelineBuilder(VkDevice device_in);

    PipelineBuilder& set_name(std::string_view name_in);

    PipelineBuilder& set_topology(VkPrimitiveTopology topology_in);

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
    PipelineBuilder& set_vertex_shader(const std::filesystem::path& vertex_path);

    PipelineBuilder& set_geometry_shader(const std::filesystem::path& geometry_path);

    PipelineBuilder& set_fragment_shader(const std::filesystem::path& fragment_path);

    PipelineBuilder& set_depth_state(const DepthStencilState& depth_stencil);

    PipelineBuilder& set_raster_state(const RasterState& raster_state_in);

    PipelineBuilder& add_blend_flag(VkPipelineColorBlendStateCreateFlagBits flag);

    PipelineBuilder& set_blend_state(uint32_t color_target_index, const VkPipelineColorBlendAttachmentState& blend);

    Pipeline build();
    
private:
    VkDevice device;

    std::string name;

    /**
     * Vertex shader SPIR-V code. If this is present, you may not load another vertex shader
     */
    tl::optional<std::vector<uint8_t>> vertex_shader;

    std::string vertex_shader_name;

    tl::optional<std::vector<uint8_t>> geometry_shader;

    std::string geometry_shader_name;

    tl::optional<std::vector<uint8_t>> fragment_shader;

    std::string fragment_shader_name;

    /**
     * Map from set number to the descriptor set info
     *
     * Map so that the vertex and fragment shaders need not have contiguous descriptor sets
     *
     * However, each set in the vertex and fragment shader must be the same - vertex shader set 0 must be the same as
     * fragment shader set 0
     */
    std::unordered_map<uint32_t, DescriptorSetInfo> descriptor_sets;

    std::vector<VkPushConstantRange> push_constants;

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {};
    
    VkPipelineRasterizationStateCreateInfo raster_state = {};

    VkPipelineColorBlendStateCreateFlags blend_flags = {};
    std::vector<VkPipelineColorBlendAttachmentState> blends = {};

    std::vector<VkVertexInputBindingDescription> vertex_inputs;
    std::vector<VkVertexInputAttributeDescription> vertex_attributes;

    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
};

/**
 * Simple material abstraction
 *
 * Materials have a PSO and descriptor sets. You can bind resources to a material, then you should finalize it
 *
 * We can't make the PSO immediately because subpasses, but we can (and do) fill out the pipeline create info and
 * allocate descriptor sets
 */
class Pipeline {
    friend class PipelineBuilder;

public:
    /**
     * Binds this pipeline to the command list
     *
     * Note: You should not call this directly. Call CommandBuffer.bind_pipeline, and it'll call this if needed
     */
    void create_vk_pipeline(const RenderBackend& backend, VkRenderPass render_pass, uint32_t subpass_index);

    VkPipeline get_vk_pipeline() const;

    VkPipelineLayout get_layout() const;

private:
    std::string pipeline_name;

    std::string vertex_shader_name;

    VkPipelineShaderStageCreateInfo vertex_stage = {};

    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    std::vector<VkVertexInputBindingDescription> vertex_inputs;

    std::vector<VkVertexInputAttributeDescription> vertex_attributes;

    std::string geometry_shader_name;

    tl::optional<VkPipelineShaderStageCreateInfo> geometry_stage = tl::nullopt;

    std::string fragment_shader_name;

    tl::optional<VkPipelineShaderStageCreateInfo> fragment_stage = tl::nullopt;

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {};

    VkPipelineRasterizationStateCreateInfo raster_state = {};

    VkPipelineColorBlendStateCreateFlags blend_flags = {};

    std::vector<VkPipelineColorBlendAttachmentState> blends = {};

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

    // Renderpass and subpass index that this pipeline was most recently used with

    VkRenderPass last_renderpass = VK_NULL_HANDLE;

    uint32_t last_subpass_index;

    VkPipeline pipeline = VK_NULL_HANDLE;

    void create_pipeline_layout(VkDevice device,
                                const std::unordered_map<uint32_t, DescriptorSetInfo>& descriptor_set_infos);
};



