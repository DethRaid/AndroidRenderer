#include "pipeline_cache.hpp"

#include "pipeline_builder.hpp"
#include "render_backend.hpp"
#include "core/system_interface.hpp"

static std::shared_ptr<spdlog::logger> logger;

PipelineCache::PipelineCache(RenderBackend& backend_in) : backend{backend_in} {
    if(logger == nullptr) {
        logger = SystemInterface::get().get_logger("PipelineCache");
    }

    const auto& physical_device = backend.get_physical_device();
    const auto data = SystemInterface::get()
                      .load_file("cache/pipeline_cache")
                      .and_then(
                          [&](const auto& cache_data) -> tl::optional<std::vector<uint8_t>> {
                              const auto* header = reinterpret_cast<const VkPipelineCacheHeaderVersionOne*>(cache_data.
                                  data());
                              if(header->vendorID == physical_device.properties.vendorID &&
                                  header->deviceID == physical_device.properties.deviceID &&
                                  std::memcmp(
                                      header->pipelineCacheUUID,
                                      physical_device.properties.pipelineCacheUUID,
                                      16) == 0) {
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
    if(vk_pipeline_cache != VK_NULL_HANDLE) {
        auto pipeline_cache_size = size_t{};
        vkGetPipelineCacheData(backend.get_device().device, vk_pipeline_cache, &pipeline_cache_size, nullptr);

        auto pipeline_cache_data = std::vector<uint8_t>{};
        pipeline_cache_data.resize(pipeline_cache_size);
        vkGetPipelineCacheData(
            backend.get_device().device,
            vk_pipeline_cache,
            &pipeline_cache_size,
            pipeline_cache_data.data()
        );

        SystemInterface::get().write_file(
            "cache/pipeline_cache",
            pipeline_cache_data.data(),
            static_cast<uint32_t>(pipeline_cache_data.size())
        );

        vkDestroyPipelineCache(backend.get_device().device, vk_pipeline_cache, nullptr);
        vk_pipeline_cache = VK_NULL_HANDLE;
    }
}

GraphicsPipelineHandle PipelineCache::create_pipeline(const GraphicsPipelineBuilder& pipeline_builder) {
    const auto device = backend.get_device().device;

    auto pipeline = GraphicsPipeline{};

    pipeline.pipeline_name = pipeline_builder.name;

    if(pipeline_builder.should_enable_dgc && backend.supports_device_generated_commands()) {
        pipeline.flags |= VK_PIPELINE_CREATE_INDIRECT_BINDABLE_BIT_NV;
    }

    if(pipeline_builder.vertex_shader) {
        ZoneScopedN("Compile vertex shader");

        const auto module_create_info = VkShaderModuleCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = static_cast<uint32_t>(pipeline_builder.vertex_shader->size()),
            .pCode = reinterpret_cast<const uint32_t*>(pipeline_builder.vertex_shader->data()),
        };
        auto vertex_module = VkShaderModule{};
        const auto result = vkCreateShaderModule(
            device,
            &module_create_info,
            nullptr,
            &vertex_module
        );
        if(result != VK_SUCCESS) {
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

    if(pipeline_builder.geometry_shader) {
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

    if(pipeline_builder.fragment_shader) {
        ZoneScopedN("Compile fragment shader");

        const auto module_create_info = VkShaderModuleCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = static_cast<uint32_t>(pipeline_builder.fragment_shader->size()),
            .pCode = reinterpret_cast<const uint32_t*>(pipeline_builder.fragment_shader->data()),
        };
        auto fragment_module = VkShaderModule{};
        vkCreateShaderModule(device, &module_create_info, nullptr, &fragment_module);

        pipeline.fragment_shader_name = pipeline_builder.fragment_shader_name;

        if(vkSetDebugUtilsObjectNameEXT != nullptr) {
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
    for(const auto& range : pipeline_builder.push_constants) {
        const auto max_used_byte = range.offset + range.size;
        pipeline.num_push_constants = std::max(pipeline.num_push_constants, max_used_byte / 4u);
        pipeline.push_constant_stages |= range.stageFlags;
        // Assumption that all shader stages will use the same push constants. If this is not true, I have a headache and I need to lie down
    }

    return pipelines.add_object(std::move(pipeline));
}

ComputePipelineHandle PipelineCache::create_pipeline(const std::string& shader_file_path) {
    const auto instructions = *SystemInterface::get().load_file(shader_file_path);

    const auto module_create_info = VkShaderModuleCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = instructions.size(),
        .pCode = reinterpret_cast<const uint32_t*>(instructions.data()),
    };
    VkShaderModule module;
    auto result = vkCreateShaderModule(backend.get_device(), &module_create_info, nullptr, &module);
    if(result != VK_SUCCESS) {
        logger->error("Could not create compute shader {}: Vulkan error {}", shader_file_path, result);
        return {};
    }

    logger->info("Beginning reflection on compute shader {}", shader_file_path);
    std::vector<DescriptorSetInfo> descriptor_sets;
    std::vector<VkPushConstantRange> push_constants;
    collect_bindings(instructions, shader_file_path, VK_SHADER_STAGE_COMPUTE_BIT, descriptor_sets, push_constants);

    // Find the greatest offset + size in the push constant ranges, assume that every other push constant is used
    uint32_t num_push_constants = 0;
    for(const auto& range : push_constants) {
        const auto max_used_byte = range.offset + range.size;
        num_push_constants = std::max(num_push_constants, max_used_byte / 4u);
    }

    auto layouts = std::vector<VkDescriptorSetLayout>{};
    layouts.reserve(descriptor_sets.size());

    for(const auto& set_info : descriptor_sets) {
        auto bindings = std::vector<VkDescriptorSetLayoutBinding>{};
        bindings.resize(set_info.bindings.size());

        for(const auto& binding : set_info.bindings) {
            bindings[binding.binding] = static_cast<VkDescriptorSetLayoutBinding>(binding);
        }

        auto create_info = VkDescriptorSetLayoutCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data(),
        };

        // If the last binding is un unsized texture array, tell Vulkan about it
        auto flags_create_info = VkDescriptorSetLayoutBindingFlagsCreateInfo{};
        auto flags = std::vector<VkDescriptorBindingFlags>{};
        if(set_info.has_variable_count_binding) {
            flags.resize(bindings.size());
            flags.back() = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
            flags_create_info = VkDescriptorSetLayoutBindingFlagsCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount = static_cast<uint32_t>(flags.size()),
                .pBindingFlags = flags.data(),
            };
            create_info.pNext = &flags_create_info;
            bindings.back().stageFlags = VK_SHADER_STAGE_ALL;
        }

        auto dsl = VkDescriptorSetLayout{};
        result = vkCreateDescriptorSetLayout(backend.get_device(), &create_info, nullptr, &dsl);
        if(result != VK_SUCCESS) {
            logger->error(
                "Could not create descriptor set layout for shader {}: Vulkan error {}",
                shader_file_path,
                result
            );
            return {};
        }

        layouts.push_back(dsl);
    }

    const auto pipeline_layout_create_info = VkPipelineLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(push_constants.size()),
        .pPushConstantRanges = push_constants.data(),
    };
    auto pipeline_layout = VkPipelineLayout{};
    result = vkCreatePipelineLayout(backend.get_device(), &pipeline_layout_create_info, nullptr, &pipeline_layout);
    if(result != VK_SUCCESS) {
        vkDestroyShaderModule(backend.get_device(), module, nullptr);
        for(const auto layout : layouts) {
            vkDestroyDescriptorSetLayout(backend.get_device(), layout, nullptr);
        }

        logger->error("Could not create pipeline layout for shader {}: Vulkan error {}", shader_file_path, result);
        return {};
    }

    const auto create_info = VkComputePipelineCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = module,
            .pName = "main",
        },
        .layout = pipeline_layout
    };
    auto pipeline = VkPipeline{};
    vkCreateComputePipelines(backend.get_device(), VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline);
    result = vkCreatePipelineLayout(backend.get_device(), &pipeline_layout_create_info, nullptr, &pipeline_layout);
    if(result != VK_SUCCESS) {
        vkDestroyShaderModule(backend.get_device(), module, nullptr);
        for(const auto layout : layouts) {
            vkDestroyDescriptorSetLayout(backend.get_device(), layout, nullptr);
        }

        vkDestroyPipelineLayout(backend.get_device(), pipeline_layout, nullptr);

        logger->error("Could not create pipeline {}: Vulkan error {}", shader_file_path, result);
        return {};
    }

    vkDestroyShaderModule(backend.get_device(), module, nullptr);
    for(const auto layout : layouts) {
        vkDestroyDescriptorSetLayout(backend.get_device(), layout, nullptr);
    }

    const auto layout_name = fmt::format("{} Layout", shader_file_path);

    backend.set_object_name(pipeline, shader_file_path);
    backend.set_object_name(pipeline_layout, layout_name);

    return compute_pipelines.add_object(
        ComputeShader{
            .name = shader_file_path,
            .layout = pipeline_layout,
            .pipeline = pipeline,
            .num_push_constants = num_push_constants,
            .descriptor_sets = descriptor_sets
        });
}

GraphicsPipelineHandle PipelineCache::create_pipeline_group(const std::span<GraphicsPipelineHandle> pipelines_in) {
    auto graphics_pipeline = GraphicsPipeline{};

    auto vk_pipelines = std::vector<VkPipeline>{};
    vk_pipelines.reserve(pipelines_in.size());
    for(const auto& pipeline : pipelines_in) {
        vk_pipelines.emplace_back(pipeline->pipeline);
    }
    const auto group_info = VkGraphicsPipelineShaderGroupsCreateInfoNV{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_SHADER_GROUPS_CREATE_INFO_NV,
        .pipelineCount = static_cast<uint32_t>(pipelines_in.size()),
        .pPipelines = vk_pipelines.data()
    };
    const auto create_info = VkGraphicsPipelineCreateInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,

        .pNext = &group_info,
    };
    vkCreateGraphicsPipelines(
        backend.get_device(),
        vk_pipeline_cache,
        1,
        &create_info,
        nullptr,
        &graphics_pipeline.pipeline);

    return pipelines.add_object(std::move(graphics_pipeline));
}

VkPipeline PipelineCache::get_pipeline_for_dynamic_rendering(
    GraphicsPipelineHandle pipeline, std::span<const VkFormat> color_attachment_formats,
    std::optional<VkFormat> depth_format, const uint32_t view_mask, const bool use_fragment_shading_rate_attachment
) const {

    ZoneScoped;

    if(pipeline->pipeline != VK_NULL_HANDLE) {
        return pipeline->pipeline;
    }

    auto stages = std::vector{pipeline->vertex_stage};
    if(pipeline->geometry_stage) {
        stages.emplace_back(*pipeline->geometry_stage);
    }
    if(pipeline->fragment_stage) {
        stages.emplace_back(*pipeline->fragment_stage);
    }

    // ReSharper disable CppVariableCanBeMadeConstexpr
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

    auto rendering_info = VkPipelineRenderingCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .viewMask = view_mask,
        .colorAttachmentCount = static_cast<uint32_t>(color_attachment_formats.size()),
        .pColorAttachmentFormats = color_attachment_formats.data(),
        .depthAttachmentFormat = depth_format.value_or(VK_FORMAT_UNDEFINED),
    };
    // ReSharper restore CppVariableCanBeMadeConstexpr

    auto create_info = VkGraphicsPipelineCreateInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering_info,

        .flags = pipeline->flags,

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

        .layout = pipeline->pipeline_layout
    };

    auto shading_rate_create_info = VkPipelineFragmentShadingRateStateCreateInfoKHR{};
    if(use_fragment_shading_rate_attachment) {
        create_info.flags |= VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;

        shading_rate_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .fragmentSize = {1, 1},
            .combinerOps = {
                VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR, VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR
            }
        };
        rendering_info.pNext = &shading_rate_create_info;
    }

    const auto device = backend.get_device().device;
    vkCreateGraphicsPipelines(
        device,
        vk_pipeline_cache,
        1,
        &create_info,
        nullptr,
        &pipeline->pipeline
    );

    if(!pipeline->pipeline_name.empty() && vkSetDebugUtilsObjectNameEXT != nullptr) {
        const auto name_info = VkDebugUtilsObjectNameInfoEXT{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = VK_OBJECT_TYPE_PIPELINE,
            .objectHandle = reinterpret_cast<uint64_t>(pipeline->pipeline),
            .pObjectName = pipeline->pipeline_name.c_str()
        };
        vkSetDebugUtilsObjectNameEXT(device, &name_info);
    }

    return pipeline->pipeline;
}

VkPipeline PipelineCache::get_pipeline(
    const GraphicsPipelineHandle pipeline, const VkRenderPass active_render_pass, const uint32_t active_subpass
) const {
    ZoneScoped;

    if(pipeline->last_renderpass == active_render_pass && pipeline->last_subpass_index == active_subpass) {
        return pipeline->pipeline;
    }

    if(pipeline->pipeline != VK_NULL_HANDLE) {
        // logger->warn("Recompiling pipeline. {} This is cringe", pipeline->pipeline_name);
    }

    auto stages = std::vector{pipeline->vertex_stage};
    if(pipeline->geometry_stage) {
        stages.emplace_back(*pipeline->geometry_stage);
    }
    if(pipeline->fragment_stage) {
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

        .flags = pipeline->flags,

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
        device,
        vk_pipeline_cache,
        1,
        &create_info,
        nullptr,
        &pipeline->pipeline
    );

    if(!pipeline->pipeline_name.empty() && vkSetDebugUtilsObjectNameEXT != nullptr) {
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
