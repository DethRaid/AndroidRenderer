#pragma once

#include "console/cvars.hpp"

#if SAH_USE_XESS
#include <string>
#include <vector>

#include <xess/xess.h>
#include <xess/xess_vk.h>

#include <vulkan/vulkan_core.h>
#include <glm/vec2.hpp>

#include "render/upscaling/upscaler.hpp"

class XeSSAdapter : public IUpscaler
{
public:
    /**
     * Retrieves the instance extension that XeSS requires
     */
    static std::vector<std::string> get_instance_extensions();

    /**
     * Retrieves the device extensions that XeSS requires
     */
    static std::vector<std::string> get_device_extensions(VkInstance instance, VkPhysicalDevice physical_device);

    /**
     * Modifies the provided Vulkan features with the features that XeSS requires
     */
    static void add_required_features(VkInstance instance, VkPhysicalDevice physical_device, VkPhysicalDeviceFeatures2& features);

    XeSSAdapter();

    ~XeSSAdapter() override;

    void initialize(glm::uvec2 output_resolution, uint32_t frame_index) override;

    glm::uvec2 get_optimal_render_resolution() const override;

    void set_constants(const SceneView& scene_view, glm::uvec2 render_resolution) override;

     void evaluate(
        RenderGraph& graph, TextureHandle color_in, TextureHandle color_out, TextureHandle depth_in,
        TextureHandle motion_vectors_in
    ) override;

private:
    xess_context_handle_t context = nullptr;

    glm::uvec2 cached_output_resolution = {};

    xess_quality_settings_t cached_quality_mode = XESS_QUALITY_SETTING_ULTRA_PERFORMANCE;

    xess_vk_execute_params_t params = {};

    xess_2d_t optimal_input_resolution = {};
    xess_2d_t min_input_resolution = {};
    xess_2d_t max_input_resolution = {};
};

#endif
