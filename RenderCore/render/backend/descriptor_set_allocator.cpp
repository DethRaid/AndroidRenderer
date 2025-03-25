#include "descriptor_set_allocator.hpp"

#include <spdlog/fmt/bundled/format.h>

#include "render/backend/ray_tracing_pipeline.hpp"

DescriptorSetAllocator::DescriptorSetAllocator(RenderBackend& backend_in) : backend{&backend_in} {}

DescriptorSetBuilder DescriptorSetAllocator::build_set(
    const GraphicsPipelineHandle pipeline, const uint32_t set_index
) {
    const auto name = fmt::format(
        "{} set {}",
        std::string_view{pipeline->name.data(), pipeline->name.size()},
        set_index);
    return build_set(pipeline->descriptor_sets.at(set_index), std::string_view{name.data(), name.size()});
}

DescriptorSetBuilder DescriptorSetAllocator::build_set(const ComputePipelineHandle pipeline, const uint32_t set_index) {
    const auto name = fmt::format(
        "{} set {}",
        std::string_view{pipeline->name.data(), pipeline->name.size()},
        set_index);
    return build_set(pipeline->descriptor_sets.at(set_index), std::string_view{name.data(), name.size()});
}

DescriptorSetBuilder DescriptorSetAllocator::build_set(const DescriptorSetInfo& info, const std::string_view name) {
    return DescriptorSetBuilder{*backend, *this, info, name};
}

DescriptorSetBuilder DescriptorSetAllocator::build_set(const RayTracingPipelineHandle pipeline, uint32_t set_index) {
    const auto name = fmt::format(
        "{} set {}",
        std::string_view{pipeline->name.data(), pipeline->name.size()},
        set_index);
    return build_set(pipeline->descriptor_sets[set_index], std::string_view{name.data(), name.size()});
}
