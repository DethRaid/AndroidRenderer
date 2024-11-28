#pragma once

#include <volk.h>
#include <absl/container/flat_hash_map.h>

struct DescriptorInfo : VkDescriptorSetLayoutBinding {
    bool is_read_only = false;
};

struct DescriptorSetInfo {
    std::vector<DescriptorInfo> bindings;

    bool has_variable_count_binding = false;
};

