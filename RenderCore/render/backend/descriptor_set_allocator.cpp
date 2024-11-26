#include "descriptor_set_allocator.hpp"

DescriptorSetAllocator::DescriptorSetAllocator(RenderBackend& backend_in) : backend{ &backend_in } {}

DescriptorSetBuilder DescriptorSetAllocator::build_set(const GraphicsPipelineHandle pipeline, const uint32_t set_index) {
    return build_set(pipeline->get_descriptor_set_info(set_index));
}

DescriptorSetBuilder DescriptorSetAllocator::build_set(const ComputePipelineHandle pipeline, const uint32_t set_index) {
    return build_set(pipeline->descriptor_sets.at(set_index));
}

DescriptorSetBuilder DescriptorSetAllocator::build_set(const DescriptorSetInfo& info) {
    return DescriptorSetBuilder{ *backend, *this, info };
}
