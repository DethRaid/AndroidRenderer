#include <vulkan/vk_enum_string_helper.h>

#include "render/backend/pipeline_cache.hpp"
#include "render/backend/pipeline_builder.hpp"
#include "render/backend/ray_tracing_pipeline.hpp"
#include "render/backend/render_backend.hpp"
#include "core/system_interface.hpp"

static std::shared_ptr<spdlog::logger> logger;

PipelineCache::PipelineCache(RenderBackend& backend_in) : backend{backend_in} {
    if(logger == nullptr) {
        logger = SystemInterface::get().get_logger("PipelineCache");
        logger->set_level(spdlog::level::debug);
    }

    const auto& physical_device = backend.get_physical_device();
    const auto data = SystemInterface::get()
                      .load_file("cache/pipeline_cache")
                      .and_then(
                          [&](const auto& cache_data) -> tl::optional<eastl::vector<std::byte>> {
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

    vkCreatePipelineCache(backend.get_device(), &create_info, nullptr, &vk_pipeline_cache);
}

PipelineCache::~PipelineCache() {
    if(vk_pipeline_cache != VK_NULL_HANDLE) {
        auto pipeline_cache_size = size_t{};
        vkGetPipelineCacheData(backend.get_device(), vk_pipeline_cache, &pipeline_cache_size, nullptr);

        auto pipeline_cache_data = eastl::vector<uint8_t>{};
        pipeline_cache_data.resize(pipeline_cache_size);
        vkGetPipelineCacheData(
            backend.get_device(),
            vk_pipeline_cache,
            &pipeline_cache_size,
            pipeline_cache_data.data()
        );

        SystemInterface::get().write_file(
            "cache/pipeline_cache",
            pipeline_cache_data.data(),
            static_cast<uint32_t>(pipeline_cache_data.size())
        );

        vkDestroyPipelineCache(backend.get_device(), vk_pipeline_cache, nullptr);
        vk_pipeline_cache = VK_NULL_HANDLE;
    }
}

GraphicsPipelineHandle PipelineCache::create_pipeline(const GraphicsPipelineBuilder& pipeline_builder) {
    auto pipeline = GraphicsPipeline{};

    pipeline.name = pipeline_builder.name;

    if(pipeline_builder.should_enable_dgc && backend.supports_device_generated_commands()) {
        pipeline.flags |= VK_PIPELINE_CREATE_INDIRECT_BINDABLE_BIT_NV;
    }

    if(!pipeline_builder.vertex_shader) {
        throw std::runtime_error{"Vertex shader is required!"};
    }
    pipeline.vertex_shader = *pipeline_builder.vertex_shader;
    if(pipeline_builder.geometry_shader) {
        pipeline.geometry_shader = *pipeline_builder.geometry_shader;
    }
    if(pipeline_builder.fragment_shader) {
        pipeline.fragment_shader = *pipeline_builder.fragment_shader;
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

    return &(*pipelines.emplace(std::move(pipeline)));
}

ComputePipelineHandle PipelineCache::create_pipeline(const std::filesystem::path& shader_file_path) {
    logger->debug("Creating compute PSO {}", shader_file_path.string());

    const auto instructions = *SystemInterface::get().load_file(shader_file_path);

    const auto module_create_info = VkShaderModuleCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = instructions.size(),
        .pCode = reinterpret_cast<const uint32_t*>(instructions.data()),
    };

    auto pipeline = ComputePipeline{};

    pipeline.name = shader_file_path.string();
    pipeline.push_constant_stages = VK_SHADER_STAGE_COMPUTE_BIT;

    eastl::fixed_vector<VkPushConstantRange, 4> push_constants;
    collect_bindings(
        instructions,
        pipeline.name,
        VK_SHADER_STAGE_COMPUTE_BIT,
        pipeline.descriptor_sets,
        push_constants);

    // Find the greatest offset + size in the push constant ranges, assume that every other push constant is used
    for(const auto& range : push_constants) {
        const auto max_used_byte = range.offset + range.size;
        pipeline.num_push_constants = std::max(pipeline.num_push_constants, max_used_byte / 4u);
    }

    pipeline.create_pipeline_layout(backend, pipeline.descriptor_sets, push_constants);

    logger->trace("Created pipeline layout");

    const auto create_info = VkComputePipelineCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = &module_create_info,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .pName = "main",
        },
        .layout = pipeline.layout
    };

    auto result = vkCreateComputePipelines(
        backend.get_device(),
        VK_NULL_HANDLE,
        1,
        &create_info,
        nullptr,
        &pipeline.pipeline);
    if(result != VK_SUCCESS) {
        logger->error("Could not create pipeline {}: Vulkan error {}", shader_file_path.string(), result);
        return {};
    }

    logger->trace("Created pipeline");

    const auto layout_name = fmt::format("{} Layout", shader_file_path.string());

    backend.set_object_name(pipeline.pipeline, pipeline.name);
    backend.set_object_name(pipeline.layout, layout_name);

    logger->trace("Named pipeline and pipeline layout");

    return &(*compute_pipelines.emplace(std::move(pipeline)));
}

GraphicsPipelineHandle PipelineCache::create_pipeline_group(const std::span<GraphicsPipelineHandle> pipelines_in) {
    auto graphics_pipeline = GraphicsPipeline{};

    auto vk_pipelines = eastl::vector<VkPipeline>{};
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

    return &(*pipelines.emplace(std::move(graphics_pipeline)));
}

VkPipeline PipelineCache::get_pipeline(
    GraphicsPipelineHandle pipeline, std::span<const VkFormat> color_attachment_formats,
    std::optional<VkFormat> depth_format, const uint32_t view_mask, const bool use_fragment_shading_rate_attachment
) const {

    ZoneScoped;

    if(pipeline->pipeline != VK_NULL_HANDLE) {
        return pipeline->pipeline;
    }

    auto stages = eastl::vector<VkPipelineShaderStageCreateInfo>{};
    stages.reserve(3);

    const auto vertex_module = VkShaderModuleCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = static_cast<uint32_t>(pipeline->vertex_shader.size()),
        .pCode = reinterpret_cast<const uint32_t*>(pipeline->vertex_shader.data()),
    };

    stages.emplace_back(
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = &vertex_module,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .pName = "main",
        });

    VkShaderModuleCreateInfo geometry_module;
    if(!pipeline->geometry_shader.empty()) {
        geometry_module = VkShaderModuleCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = static_cast<uint32_t>(pipeline->geometry_shader.size()),
            .pCode = reinterpret_cast<const uint32_t*>(pipeline->geometry_shader.data()),
        };

        stages.emplace_back(
            VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = &geometry_module,
                .stage = VK_SHADER_STAGE_GEOMETRY_BIT,
                .pName = "main",
            });
    }

    VkShaderModuleCreateInfo fragment_module;
    if(!pipeline->fragment_shader.empty()) {
        fragment_module = VkShaderModuleCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = static_cast<uint32_t>(pipeline->fragment_shader.size()),
            .pCode = reinterpret_cast<const uint32_t*>(pipeline->fragment_shader.data()),
        };

        stages.emplace_back(
            VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = &fragment_module,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pName = "main",
            });
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

    const auto dynamic_states = eastl::array{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_FRONT_FACE,
        VK_DYNAMIC_STATE_CULL_MODE
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

        .layout = pipeline->layout
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

    const auto& device = backend.get_device();
    logger->trace("About to compile PSO {}", pipeline->name);
    const auto result = vkCreateGraphicsPipelines(
        device,
        vk_pipeline_cache,
        1,
        &create_info,
        nullptr,
        &pipeline->pipeline
    );
    if(result != VK_SUCCESS) {
        logger->error("Could not create pipeline {}: {}", pipeline->name, string_VkResult(result));
    }

    if(!pipeline->name.empty()) {
        backend.set_object_name(pipeline->pipeline, pipeline->name);
    }

    return pipeline->pipeline;
}

void PipelineCache::add_miss_shaders(
    const std::span<const std::byte> occlusion_miss, const std::span<const std::byte> gi_miss
) {
    occlusion_miss_shader.resize(occlusion_miss.size());
    std::memcpy(occlusion_miss_shader.data(), occlusion_miss.data(), occlusion_miss.size_bytes());

    gi_miss_shader.resize(gi_miss.size());
    std::memcpy(gi_miss_shader.data(), gi_miss.data(), gi_miss.size_bytes());
}

HitGroupHandle PipelineCache::add_hit_group(const HitGroupBuilder& shader_group) {
    return &(*shader_groups.emplace(
        HitGroup{
            .name = shader_group.name,
            .index = static_cast<uint32_t>(shader_groups.size()),
            .occlusion_anyhit_shader = shader_group.occlusion_anyhit_shader,
            .occlusion_closesthit_shader = shader_group.occlusion_closesthit_shader,
            .gi_anyhit_shader = shader_group.gi_anyhit_shader,
            .gi_closesthit_shader = shader_group.gi_closesthit_shader
        }));
}

static uint32_t round_up(const uint32_t num, const uint32_t multiple) {
    if(multiple == 0) {
        return num;
    }

    auto remainder = num % multiple;
    if(remainder == 0) {
        return num;
    }

    return num + multiple - remainder;
}

RayTracingPipelineHandle PipelineCache::create_ray_tracing_pipeline(
    const std::filesystem::path& raygen_shader_path, bool skip_gi_miss_shader
) {
    ZoneScoped;

    logger->debug("Creating RT PSO {}", raygen_shader_path.string());

    auto pipeline = RayTracingPipeline{};

    const auto& device = backend.get_device();

    // Reserve enough space for both a closesthit and anyhit for both GI and occlusion - but non-masked materials don't
    // have anyhit shaders
    auto stages = eastl::vector<VkPipelineShaderStageCreateInfo>{};
    stages.reserve(shader_groups.size() * 4 + 2);

    auto groups = eastl::vector<VkRayTracingShaderGroupCreateInfoKHR>{};
    groups.reserve(shader_groups.size() * 2 + 2);

    auto modules = eastl::vector<VkShaderModuleCreateInfo>{};
    modules.reserve(stages.capacity());

    eastl::fixed_vector<VkPushConstantRange, 4> push_constants;

    // Add stages for each shader group, and add two groups for each shader group. Occlusion is first, GI is second
    for(const auto& shader_group : shader_groups) {
        // Occlusion 
        {
            auto occlusion_closesthit_index = VK_SHADER_UNUSED_KHR;
            auto occlusion_anyhit_index = VK_SHADER_UNUSED_KHR;

            // Occlusion stages
            const auto& occlusion_anyhit_shader = shader_group.occlusion_anyhit_shader;
            if(!occlusion_anyhit_shader.empty()) {
                occlusion_anyhit_index = static_cast<uint32_t>(stages.size());

                const auto& module_create_info = modules.emplace_back(
                    VkShaderModuleCreateInfo{
                        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                        .codeSize = occlusion_anyhit_shader.size() * sizeof(uint8_t),
                        .pCode = reinterpret_cast<const uint32_t*>(occlusion_anyhit_shader.data())
                    });

                stages.emplace_back(
                    VkPipelineShaderStageCreateInfo{
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .pNext = &module_create_info,
                        .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
                        .pName = "main"
                    });

                collect_bindings(
                    occlusion_anyhit_shader,
                    shader_group.name,
                    VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
                    pipeline.descriptor_sets,
                    push_constants);
            }

            const auto& occlusion_closesthit_shader = shader_group.occlusion_closesthit_shader;
            if(!occlusion_closesthit_shader.empty()) {
                occlusion_closesthit_index = static_cast<uint32_t>(stages.size());

                const auto& module_create_info = modules.emplace_back(
                    VkShaderModuleCreateInfo{
                        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                        .codeSize = occlusion_closesthit_shader.size() * sizeof(uint8_t),
                        .pCode = reinterpret_cast<const uint32_t*>(occlusion_closesthit_shader.data())
                    });

                stages.emplace_back(
                    VkPipelineShaderStageCreateInfo{
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .pNext = &module_create_info,
                        .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                        .pName = "main"
                    });

                collect_bindings(
                    occlusion_closesthit_shader,
                    shader_group.name,
                    VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                    pipeline.descriptor_sets,
                    push_constants);
            }

            // Occlusion group
            groups.emplace_back(
                VkRayTracingShaderGroupCreateInfoKHR{
                    .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                    .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
                    .generalShader = VK_SHADER_UNUSED_KHR,
                    .closestHitShader = occlusion_closesthit_index,
                    .anyHitShader = occlusion_anyhit_index,
                    .intersectionShader = VK_SHADER_UNUSED_KHR,
                });
        }

        // GI
        {
            // GI stages

            auto gi_closesthit_index = VK_SHADER_UNUSED_KHR;
            auto gi_anyhit_index = VK_SHADER_UNUSED_KHR;

            const auto& gi_anyhit_shader = shader_group.gi_anyhit_shader;
            if(!gi_anyhit_shader.empty()) {
                gi_anyhit_index = static_cast<uint32_t>(stages.size());

                const auto& module_create_info = modules.emplace_back(
                    VkShaderModuleCreateInfo{
                        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                        .codeSize = gi_anyhit_shader.size() * sizeof(uint8_t),
                        .pCode = reinterpret_cast<const uint32_t*>(gi_anyhit_shader.data())
                    });

                stages.emplace_back(
                    VkPipelineShaderStageCreateInfo{
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .pNext = &module_create_info,
                        .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
                        .pName = "main"
                    });

                collect_bindings(
                    gi_anyhit_shader,
                    shader_group.name,
                    VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
                    pipeline.descriptor_sets,
                    push_constants);
            }

            const auto& gi_closesthit_shader = shader_group.gi_closesthit_shader;
            if(!gi_closesthit_shader.empty()) {
                gi_closesthit_index = static_cast<uint32_t>(stages.size());

                const auto& module_create_info = modules.emplace_back(
                    VkShaderModuleCreateInfo{
                        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                        .codeSize = gi_closesthit_shader.size() * sizeof(uint8_t),
                        .pCode = reinterpret_cast<const uint32_t*>(gi_closesthit_shader.data())
                    });

                stages.emplace_back(
                    VkPipelineShaderStageCreateInfo{
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .pNext = &module_create_info,
                        .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                        .pName = "main"
                    });

                collect_bindings(
                    gi_closesthit_shader,
                    shader_group.name,
                    VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                    pipeline.descriptor_sets,
                    push_constants);
            }

            // GI group
            groups.emplace_back(
                VkRayTracingShaderGroupCreateInfoKHR{
                    .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                    .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
                    .generalShader = VK_SHADER_UNUSED_KHR,
                    .closestHitShader = gi_closesthit_index,
                    .anyHitShader = gi_anyhit_index,
                    .intersectionShader = VK_SHADER_UNUSED_KHR,
                });
        }
    }

    const auto miss_shader_index = static_cast<uint32_t>(stages.size());
    const auto miss_group_index = static_cast<uint32_t>(groups.size());

    // Occlusion miss
    {
        const auto& miss_module_create_info = modules.emplace_back(
            VkShaderModuleCreateInfo{
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = occlusion_miss_shader.size() * sizeof(uint8_t),
                .pCode = reinterpret_cast<const uint32_t*>(occlusion_miss_shader.data())
            });

        stages.emplace_back(
            VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = &miss_module_create_info,
                .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
                .pName = "main"
            });

        groups.emplace_back(
            VkRayTracingShaderGroupCreateInfoKHR{
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                .generalShader = miss_shader_index,
                .closestHitShader = VK_SHADER_UNUSED_KHR,
                .anyHitShader = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR,
            });

        collect_bindings(
            occlusion_miss_shader,
            "Occlusion miss shader",
            VK_SHADER_STAGE_MISS_BIT_KHR,
            pipeline.descriptor_sets,
            push_constants);
    }

    auto num_miss_shaders = 1u;
    // GI miss
    if(!skip_gi_miss_shader) {
        num_miss_shaders = 2;
        const auto& miss_module_create_info = modules.emplace_back(
            VkShaderModuleCreateInfo{
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = gi_miss_shader.size() * sizeof(uint8_t),
                .pCode = reinterpret_cast<const uint32_t*>(gi_miss_shader.data())
            });

        stages.emplace_back(
            VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = &miss_module_create_info,
                .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
                .pName = "main"
            });

        groups.emplace_back(
            VkRayTracingShaderGroupCreateInfoKHR{
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                .generalShader = miss_shader_index + 1,
                .closestHitShader = VK_SHADER_UNUSED_KHR,
                .anyHitShader = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR,
            });

        collect_bindings(
            gi_miss_shader,
            "GI miss shader",
            VK_SHADER_STAGE_MISS_BIT_KHR,
            pipeline.descriptor_sets,
            push_constants);
    }

    const auto raygen_shader_index = miss_shader_index + num_miss_shaders;
    const auto raygen_group_index = static_cast<uint32_t>(groups.size());

    const auto raygen_shader_maybe = SystemInterface::get().load_file(raygen_shader_path);
    if(!raygen_shader_maybe) {
        throw std::runtime_error{fmt::format("Could not load raygen shader {}", raygen_shader_path.string())};
    }
    const auto& raygen_module_create_info = modules.emplace_back(
        VkShaderModuleCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = raygen_shader_maybe->size() * sizeof(uint8_t),
            .pCode = reinterpret_cast<const uint32_t*>(raygen_shader_maybe->data())
        });

    stages.emplace_back(
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = &raygen_module_create_info,
            .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            .pName = "main"
        });

    groups.emplace_back(
        VkRayTracingShaderGroupCreateInfoKHR{
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = raygen_shader_index,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
        });

    const auto raygen_shader_name = raygen_shader_path.string();
    collect_bindings(
        *raygen_shader_maybe,
        std::string_view{raygen_shader_name.c_str()},
        VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        pipeline.descriptor_sets,
        push_constants);

    // Find the greatest offset + size in the push constant ranges, assume that every other push constant is used
    for(auto& range : push_constants) {
        range.stageFlags = VK_SHADER_STAGE_ALL;
        const auto max_used_byte = range.offset + range.size;
        pipeline.num_push_constants = std::max(pipeline.num_push_constants, max_used_byte / 4u);
    }

    pipeline.create_pipeline_layout(backend, pipeline.descriptor_sets, push_constants);

    constexpr auto lib_interface = VkRayTracingPipelineInterfaceCreateInfoKHR{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_INTERFACE_CREATE_INFO_KHR,
        .maxPipelineRayPayloadSize = 32,
        .maxPipelineRayHitAttributeSize = sizeof(glm::vec2)
    };

    const auto create_info = VkRayTracingPipelineCreateInfoKHR{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .groupCount = static_cast<uint32_t>(groups.size()),
        .pGroups = groups.data(),
        .maxPipelineRayRecursionDepth = 8,
        .pLibraryInterface = &lib_interface,
        .layout = pipeline.layout
    };

    auto result = vkCreateRayTracingPipelinesKHR(
        device,
        VK_NULL_HANDLE,
        vk_pipeline_cache,
        1,
        &create_info,
        nullptr,
        &pipeline.pipeline);
    if(result != VK_SUCCESS) {
        logger->error(
            "Could not create ray tracing pipeline {}: {}",
            raygen_shader_path.string(),
            string_VkResult(result));
        return nullptr;
    }

    const auto shader_group_handle_size = backend.get_shader_group_handle_size();
    const auto shader_group_alignment = backend.get_shader_group_alignment();

    auto shader_group_handles = eastl::vector<uint8_t>{};

    const auto hit_table_size = round_up(groups.size() * shader_group_handle_size, shader_group_alignment);
    const auto miss_table_size = round_up(shader_group_handle_size * num_miss_shaders, shader_group_alignment);
    const auto raygen_table_size = round_up(shader_group_handle_size, shader_group_alignment);

    shader_group_handles.resize(hit_table_size + miss_table_size + raygen_table_size);

    auto* write_ptr = shader_group_handles.data();

    // Hit groups shader groups
    result = vkGetRayTracingShaderGroupHandlesKHR(
        device,
        pipeline.pipeline,
        0,
        static_cast<uint32_t>(groups.size()),
        hit_table_size,
        write_ptr);
    if(result != VK_SUCCESS) {
        logger->error("Could not retrieve hit groups handles");
    }

    write_ptr += hit_table_size;

    // Raygen shader handles
    result = vkGetRayTracingShaderGroupHandlesKHR(
        device,
        pipeline.pipeline,
        raygen_group_index,
        1,
        raygen_table_size,
        write_ptr);
    if(result != VK_SUCCESS) {
        logger->error("Could not retrieve raygen groups handles");
    }

    write_ptr += raygen_table_size;

    // Miss shader handles
    result = vkGetRayTracingShaderGroupHandlesKHR(
        device,
        pipeline.pipeline,
        miss_group_index,
        num_miss_shaders,
        miss_table_size,
        write_ptr);
    if(result != VK_SUCCESS) {
        logger->error("Could not retrieve miss groups handles");
    }

    const auto buffer_name = fmt::format("{} shader tables", raygen_shader_path.string());
    pipeline.shader_tables_buffer = backend.get_global_allocator().create_buffer(
        buffer_name.c_str(),
        shader_group_handles.size(),
        BufferUsage::ShaderBindingTable
    );

    backend.get_upload_queue().upload_to_buffer(pipeline.shader_tables_buffer, std::span{shader_group_handles});

    pipeline.raygen_table = {
        .deviceAddress = pipeline.shader_tables_buffer->address + hit_table_size,
        .stride = shader_group_handle_size,
        .size = shader_group_handle_size
    };

    pipeline.hit_table = {
        .deviceAddress = pipeline.shader_tables_buffer->address,
        .stride = shader_group_handle_size,
        .size = groups.size() * shader_group_handle_size
    };

    pipeline.miss_table = {
        .deviceAddress = pipeline.shader_tables_buffer->address + hit_table_size + raygen_table_size,
        .stride = shader_group_handle_size,
        .size = shader_group_handle_size
    };

    return &(*ray_tracing_pipelines.emplace(std::move(pipeline)));
}
