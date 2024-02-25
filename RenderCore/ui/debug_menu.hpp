#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include "render/backend/handles.hpp"
#include "render/visualizers/visualizer_type.hpp"

class SceneRenderer;
class ResourceAllocator;

class DebugUI {
public:
    explicit DebugUI(SceneRenderer& renderer_in);

    void draw();

private:
    GLFWwindow* window = nullptr;

    SceneRenderer& renderer;

    ResourceAllocator& allocator;

    bool is_debug_menu_open = true;

    double last_start_time = 0.0;

    TextureHandle font_atlas_handle;

    bool imgui_demo_open = true;

    VkDescriptorSet font_atlas_descriptor_set;

    RenderVisualization selected_visualizer;

    void create_font_texture();

#if defined(_WIN32)
    void update_mouse_pos_and_buttons() const;

    void update_mouse_cursor() const;
#endif

    void draw_debug_menu();
};
