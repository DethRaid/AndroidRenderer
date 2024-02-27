#pragma once

#include <unordered_map>
#include <volk.h>

struct DescriptorInfo : VkDescriptorSetLayoutBinding {
    bool is_read_only = false;
};

struct DescriptorSetInfo {
    std::unordered_map<uint32_t, DescriptorInfo> bindings;

    bool has_variable_count_binding = false;
};

