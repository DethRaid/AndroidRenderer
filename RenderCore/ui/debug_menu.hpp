#pragma once

#include <vulkan/vulkan.h>

#if defined(_WIN32)
#include <GLFW/glfw3.h>
#endif

#include <span>
#include <string>

#include "render/backend/handles.hpp"
#include "render/visualizers/visualizer_type.hpp"

class SceneRenderer;
class ResourceAllocator;

class DebugUI {
public:
    explicit DebugUI(SceneRenderer& renderer_in);

    void draw();

private:
#if defined(_WIN32)
    GLFWwindow* window = nullptr;
#endif

    SceneRenderer& renderer;

    ResourceAllocator& allocator;

    bool is_debug_menu_open = true;

    double last_start_time = 0.0;

    TextureHandle font_atlas_handle;

    bool imgui_demo_open = true;

    VkDescriptorSet font_atlas_descriptor_set;

    int current_taa = 0;

    int current_dlss_mode = 1;

    int current_xess_mode = 1;

    int current_fsr_mode = 1;

    bool use_ray_reconstruction = false;

    int selected_gi_quality = 1;

    void create_font_texture();

#if defined(_WIN32)
    void update_mouse_pos_and_buttons() const;

    void update_mouse_cursor() const;
#endif

    void draw_debug_menu();

    void draw_taa_menu();

    void draw_gi_menu();

    static void draw_combo_box(const std::string& name, std::span<const std::string> items, int& selected_item);
};
