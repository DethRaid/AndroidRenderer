#pragma once

#include <sl_core_types.h>
#include <sl_result.h>
#include <string>
#include <vulkan/vulkan_core.h>

class StreamlineAdapter
{
public:
    static PFN_vkGetInstanceProcAddr try_load_streamline();

    static bool is_available();

    StreamlineAdapter();

    bool is_dlss_supported() const;

    /**
     * Updates the internal frame token, which is used by various Streamline features
     *
     * This should be called at the very beginning of simulation
     *
     * @param frame_index Index of the frame
     */
    void update_frame_token(uint32_t frame_index);

    ~StreamlineAdapter();

private:
    static inline bool available = false;

    sl::Result dlss_support;

    sl::FrameToken* frame_token = nullptr;
};

std::string to_string(sl::Result result);
