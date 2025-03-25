#pragma once

#include <EASTL/vector.h>

#include <volk.h>

struct DescriptorInfo : VkDescriptorSetLayoutBinding {
    bool is_read_only = false;
};

struct DescriptorSetInfo {
    eastl::vector<DescriptorInfo> bindings;

    bool has_variable_count_binding = false;
};

