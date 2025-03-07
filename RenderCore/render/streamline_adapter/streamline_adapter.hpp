#pragma once

#if SAH_USE_STREAMLINE

#include <string>

#include <glm/vec2.hpp>
#include <sl.h>
#include <sl_dlss.h>
#include <vulkan/vulkan_core.h>

#include "render/scene_renderer.hpp"

class StreamlineAdapter {
public:
    static PFN_vkGetInstanceProcAddr try_load_streamline();

    static bool is_available();

    StreamlineAdapter();

    ~StreamlineAdapter();

    void set_devices_from_backend(const RenderBackend& backend);

    bool is_dlss_supported() const;

    /**
     * Updates the internal frame token, which is used by various Streamline features
     *
     * This should be called at the very beginning of simulation
     *
     * @param frame_index Index of the frame
     */
    void update_frame_token(uint32_t frame_index);

    void set_constants(const SceneView& scene_transform, glm::uvec2 render_resolution);

    /**
     * Sets the DLSS mode to use
     *
     * Should be called before any other DLSS-related methods
     *
     * @param mode DLSS mode. Pass
     */
    void set_dlss_mode(sl::DLSSMode mode);

    /**
     * Gets the optimal render resolution for the current DLSS mode
     *
     * @param output_resolution Resolution of the output (swapchain, usually)
     * @return Optimal resolution to render at
     * @see set_dlss_mode
     */
    glm::uvec2 get_dlss_render_resolution(const glm::uvec2& output_resolution);

    void evaluate_dlss(CommandBuffer& commands,
        TextureHandle color_in, TextureHandle color_out,
        TextureHandle depth_in, TextureHandle motion_vectors_in);

private:
    static inline bool available = false;

    // I guess what do to?
    sl::ViewportHandle viewport = { 0 };

    sl::Result dlss_support;

    sl::DLSSMode dlss_mode = sl::DLSSMode::eDLAA;
    sl::DLSSOptimalSettings dlss_settings = {};

    sl::FrameToken* frame_token = nullptr;
};
#endif
