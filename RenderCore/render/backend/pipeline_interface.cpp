#include "pipeline_interface.hpp"

#include "render/backend/render_backend.hpp"

PipelineBase::~PipelineBase() {
    auto& backend = RenderBackend::get();

    if(layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(backend.get_device(), layout, nullptr);
    }

    if(pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(backend.get_device(), pipeline, nullptr);
    }
}

PipelineBase::PipelineBase(PipelineBase&& old) noexcept : name{std::move(old.name)},
                                                          pipeline{old.pipeline},
                                                          layout{old.layout},
                                                          num_push_constants{old.num_push_constants},
                                                          push_constant_stages{old.push_constant_stages},
                                                          descriptor_sets{std::move(old.descriptor_sets)},
                                                          descriptor_set_layouts{
                                                              std::move(old.descriptor_set_layouts)
                                                          } {
    old.pipeline = VK_NULL_HANDLE;
    old.layout = VK_NULL_HANDLE;
}

PipelineBase& PipelineBase::operator=(PipelineBase&& old) noexcept {
    this->~PipelineBase();

    name = std::move(old.name);
    pipeline = old.pipeline;
    layout = old.layout;
    num_push_constants = old.num_push_constants;
    push_constant_stages = old.push_constant_stages;
    descriptor_sets = std::move(old.descriptor_sets);
    descriptor_set_layouts = std::move(old.descriptor_set_layouts);

    old.pipeline = VK_NULL_HANDLE;
    old.layout = VK_NULL_HANDLE;

    return *this;
}

void PipelineBase::create_pipeline_layout(
    RenderBackend& backend, const eastl::vector<DescriptorSetInfo>& descriptor_set_infos,
    const eastl::vector<VkPushConstantRange>& push_constants
) {
    // Create descriptor sets
    descriptor_set_layouts.resize(descriptor_set_infos.size());

    auto& cache = backend.get_descriptor_cache();

    auto set_index = 0u;
    for(const auto& set_info : descriptor_set_infos) {
        auto bindings = eastl::vector<VkDescriptorSetLayoutBinding>{};

        for(const auto& binding : set_info.bindings) {
            bindings.emplace_back(binding);
        }

        auto create_info = VkDescriptorSetLayoutCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data(),
        };

        // If the last binding is un unsized texture array, tell Vulkan about it
        auto flags_create_info = VkDescriptorSetLayoutBindingFlagsCreateInfo{};
        auto binding_flags = eastl::vector<VkDescriptorBindingFlags>{};
        if(set_info.has_variable_count_binding) {
            binding_flags.resize(bindings.size());
            binding_flags.back() = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
            flags_create_info = VkDescriptorSetLayoutBindingFlagsCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount = static_cast<uint32_t>(binding_flags.size()),
                .pBindingFlags = binding_flags.data(),
            };
            create_info.pNext = &flags_create_info;
            bindings.back().stageFlags = VK_SHADER_STAGE_ALL;
        }

        const auto descriptor_set_layout = cache.create_descriptor_layout(&create_info);

        descriptor_set_layouts[set_index] = descriptor_set_layout;

        set_index++;
    }

    const auto create_info = VkPipelineLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts.size()),
        .pSetLayouts = descriptor_set_layouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(push_constants.size()),
        .pPushConstantRanges = push_constants.data(),
    };

    vkCreatePipelineLayout(backend.get_device(), &create_info, nullptr, &layout);

    if(!name.empty()) {
        backend.set_object_name(layout, name);
    }
}
