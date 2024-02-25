#include "pipeline_cache.hpp"

#include "pipeline_builder.hpp"
#include "render_backend.hpp"
#include "core/system_interface.hpp"

static std::shared_ptr<spdlog::logger> logger;

PipelineCache::PipelineCache(RenderBackend& backend_in) : backend{backend_in} {
    if (logger == nullptr) {
        logger = SystemInterface::get().get_logger("PipelineCache");
    }

    const auto& physical_device = backend.get_physical_device();
    const auto data = SystemInterface::get()
                      .load_file("cache/pipeline_cache")
                      .and_then(
                          [&](const auto& cache_data) -> tl::optional<std::vector<uint8_t>> {
                              const auto* header = reinterpret_cast<const VkPipelineCacheHeaderVersionOne*>(cache_data.
                                  data());
                              if (header->vendorID == physical_device.properties.vendorID &&
                                  header->deviceID == physical_device.properties.deviceID &&
                                  std::memcmp(header->pipelineCacheUUID, physical_device.properties.pipelineCacheUUID, 16) == 0) {
                                  return cache_data;
                              }

                              return tl::nullopt;
                          }
                      );

    const auto create_info = VkPipelineCacheCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .initialDataSize = data ? data->size() : 0,
        .pInitialData = data ? data->data() : nullptr,
    };

    vkCreatePipelineCache(backend.get_device().device, &create_info, nullptr, &vk_pipeline_cache);
}

PipelineCache::~PipelineCache() {
    if (vk_pipeline_cache != VK_NULL_HANDLE) {
        auto pipeline_cache_size = size_t{};
        vkGetPipelineCacheData(backend.get_device().device, vk_pipeline_cache, &pipeline_cache_size, nullptr);

        auto pipeline_cache_data = std::vector<uint8_t>{};
        pipeline_cache_data.resize(pipeline_cache_size);
        vkGetPipelineCacheData(
            backend.get_device().device, vk_pipeline_cache, &pipeline_cache_size, pipeline_cache_data.data()
        );

        SystemInterface::get().write_file(
            "cache/pipeline_cache", pipeline_cache_data.data(), static_cast<uint32_t>(pipeline_cache_data.size())
        );

        vkDestroyPipelineCache(backend.get_device().device, vk_pipeline_cache, nullptr);
        vk_pipeline_cache = VK_NULL_HANDLE;
    }
}

GraphicsPipelineHandle PipelineCache::create_pipeline(const GraphicsPipelineBuilder& pipeline_builder) {
    const auto device = backend.get_device().device;

    auto pipeline = GraphicsPipeline{};

    pipeline.pipeline_name = pipeline_builder.name;

    if (pipeline_builder.vertex_shader) {
        ZoneScopedN("Compile vertex shader");

        const auto module_create_info = VkShaderModuleCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = static_cast<uint32_t>(pipeline_builder.vertex_shader->size()),
            .pCode = reinterpret_cast<const uint32_t*>(pipeline_builder.vertex_shader->data()),
        };
        auto vertex_module = VkShaderModule{};
        const auto result = vkCreateShaderModule(
            device, &module_create_info, nullptr,
            &vertex_module
        );
        if (result != VK_SUCCESS) {
            throw std::runtime_error{"Could not create vertex module"};
        }

        pipeline.vertex_shader_name = pipeline_builder.vertex_shader_name;

        backend.set_object_name(vertex_module, pipeline_builder.vertex_shader_name);

        pipeline.vertex_stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertex_module,
            .pName = "main",
        };
    } else {
        throw std::runtime_error{"Missing vertex shader"};
    }

    if (pipeline_builder.geometry_shader) {
        ZoneScopedN("Compile geometry shader");

        const auto module_create_info = VkShaderModuleCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = static_cast<uint32_t>(pipeline_builder.geometry_shader->size()),
            .pCode = reinterpret_cast<const uint32_t*>(pipeline_builder.geometry_shader->data()),
        };
        auto geometry_module = VkShaderModule{};
        vkCreateShaderModule(device, &module_create_info, nullptr, &geometry_module);

        pipeline.geometry_shader_name = pipeline_builder.geometry_shader_name;

        backend.set_object_name(geometry_module, pipeline_builder.geometry_shader_name);

        pipeline.geometry_stage = VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_GEOMETRY_BIT,
            .module = geometry_module,
            .pName = "main",
        };
    }

    if (pipeline_builder.fragment_shader) {
        ZoneScopedN("Compile fragment shader");

        const auto module_create_info = VkShaderModuleCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = static_cast<uint32_t>(pipeline_builder.fragment_shader->size()),
            .pCode = reinterpret_cast<const uint32_t*>(pipeline_builder.fragment_shader->data()),
        };
        auto fragment_module = VkShaderModule{};
        vkCreateShaderModule(device, &module_create_info, nullptr, &fragment_module);

        pipeline.fragment_shader_name = pipeline_builder.fragment_shader_name;

        if (vkSetDebugUtilsObjectNameEXT != nullptr) {
            const auto name_info = VkDebugUtilsObjectNameInfoEXT{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .objectType = VK_OBJECT_TYPE_SHADER_MODULE,
                .objectHandle = reinterpret_cast<uint64_t>(fragment_module),
                .pObjectName = pipeline_builder.fragment_shader_name.c_str()
            };
            vkSetDebugUtilsObjectNameEXT(device, &name_info);
        }

        pipeline.fragment_stage = VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragment_module,
            .pName = "main",
        };
    }

    pipeline.depth_stencil_state = pipeline_builder.depth_stencil_state;
    pipeline.raster_state = pipeline_builder.raster_state;
    pipeline.blend_flags = pipeline_builder.blend_flags;
    pipeline.blends = pipeline_builder.blends;

    pipeline.topology = pipeline_builder.topology;
    pipeline.vertex_inputs = pipeline_builder.vertex_inputs;
    pipeline.vertex_attributes = pipeline_builder.vertex_attributes;

    pipeline.create_pipeline_layout(backend, pipeline_builder.descriptor_sets, pipeline_builder.push_constants);
    pipeline.descriptor_sets = pipeline_builder.descriptor_sets;

    // Find the greatest offset + size in the push constant ranges, assume that every other push constant is used
    pipeline.num_push_constants = 0;
    for (const auto& range : pipeline_builder.push_constants) {
        const auto max_used_byte = range.offset + range.size;
        pipeline.num_push_constants = std::max(pipeline.num_push_constants, max_used_byte / 4u);
        pipeline.push_constant_stages |= range.stageFlags;  // Assumption that all shader stages will use the same push constants. If this is not true, I have a headache and I need to lie down
    }

    return pipelines.add_object(std::move(pipeline));
}

VkPipeline PipelineCache::get_pipeline(
    const GraphicsPipelineHandle pipeline, const VkRenderPass active_render_pass, const uint32_t active_subpass
) const {
    ZoneScoped;

    if (pipeline->last_renderpass == active_render_pass && pipeline->last_subpass_index == active_subpass) {
        return pipeline->pipeline;
    }

    if (pipeline->pipeline != VK_NULL_HANDLE) {
        // logger->warn("Recompiling pipeline. {} This is cringe", pipeline->pipeline_name);
    }

    auto stages = std::vector{pipeline->vertex_stage};
    if (pipeline->geometry_stage) {
        stages.emplace_back(*pipeline->geometry_stage);
    }
    if (pipeline->fragment_stage) {
        stages.emplace_back(*pipeline->fragment_stage);
    }

    const auto vertex_input_stage = VkPipelineVertexInputStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = static_cast<uint32_t>(pipeline->vertex_inputs.size()),
        .pVertexBindingDescriptions = pipeline->vertex_inputs.data(),
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(pipeline->vertex_attributes.size()),
        .pVertexAttributeDescriptions = pipeline->vertex_attributes.data(),
    };

    const auto input_assembly_state = VkPipelineInputAssemblyStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = pipeline->topology,
    };

    const auto viewport_state = VkPipelineViewportStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
        // Dynamic viewport and scissor state
    };

    const auto multisample_state = VkPipelineMultisampleStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    const auto color_blend_state = VkPipelineColorBlendStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .flags = pipeline->blend_flags,
        .attachmentCount = static_cast<uint32_t>(pipeline->blends.size()),
        .pAttachments = pipeline->blends.data(),
    };

    const auto dynamic_states = std::array{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    const auto dynamic_state = VkPipelineDynamicStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data(),
    };

    auto create_info = VkGraphicsPipelineCreateInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,

        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),

        .pVertexInputState = &vertex_input_stage,
        .pInputAssemblyState = &input_assembly_state,

        .pViewportState = &viewport_state,

        .pRasterizationState = &pipeline->raster_state,
        .pMultisampleState = &multisample_state,

        .pDepthStencilState = &pipeline->depth_stencil_state,

        .pColorBlendState = &color_blend_state,

        .pDynamicState = &dynamic_state,

        .layout = pipeline->pipeline_layout,

        .renderPass = active_render_pass,
        .subpass = active_subpass,
    };

    const auto device = backend.get_device().device;
    vkCreateGraphicsPipelines(
        device, vk_pipeline_cache, 1, &create_info, nullptr,
        &pipeline->pipeline
    );

    if (!pipeline->pipeline_name.empty() && vkSetDebugUtilsObjectNameEXT != nullptr) {
        const auto name_info = VkDebugUtilsObjectNameInfoEXT{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = VK_OBJECT_TYPE_PIPELINE,
            .objectHandle = reinterpret_cast<uint64_t>(pipeline->pipeline),
            .pObjectName = pipeline->pipeline_name.c_str()
        };
        vkSetDebugUtilsObjectNameEXT(device, &name_info);
    }

    pipeline->last_renderpass = active_render_pass;
    pipeline->last_subpass_index = active_subpass;

    // logger->warn("Compiling pipeline {}", pipeline->pipeline_name);

    return pipeline->pipeline;
}
