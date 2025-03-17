#pragma once

#if SAH_USE_STREAMLINE
#include <vulkan/vulkan_core.h>

class StreamlineAdapter
{
public:
    StreamlineAdapter();

    ~StreamlineAdapter();

    PFN_vkGetInstanceProcAddr try_load_interposer();
};
#endif
