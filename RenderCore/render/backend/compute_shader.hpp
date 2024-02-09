#pragma once

#include <string>
#include <volk.h>
#include <tl/optional.hpp>

class RenderBackend;

struct ComputeShader {
    static tl::optional<ComputeShader> create(const RenderBackend& backend, const std::string& name, const std::vector<uint8_t>& instructions);

    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    uint32_t num_push_constants = 0;
};



