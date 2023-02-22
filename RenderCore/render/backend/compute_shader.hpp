#pragma once

#include <string>
#include <volk.h>
#include <tl/optional.hpp>

struct ComputeShader {
    static tl::optional<ComputeShader> create(VkDevice device, const std::string& name, const std::vector<uint8_t>& instructions);

    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};



