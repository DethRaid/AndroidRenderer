#include "descriptor_set_allocator.hpp"

DescriptorSetAllocator::DescriptorSetAllocator(RenderBackend& backend_in) : backend{ &backend_in } {}

DescriptorSet DescriptorSetAllocator::create_set(const GraphicsPipelineHandle pipeline, const uint32_t set_index) {
    return DescriptorSet(*backend, *this, pipeline->get_descriptor_set_info(set_index));
}

DescriptorSet DescriptorSetAllocator::create_set(const ComputePipelineHandle pipeline, const uint32_t set_index) {
    return DescriptorSet(*backend, *this, pipeline->descriptor_sets.at(set_index));
}
