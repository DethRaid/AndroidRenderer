#include "debug_menu.hpp"

#include <imgui.h>
#include <magic_enum.hpp>

#include "console/cvars.hpp"
#include "core/system_interface.hpp"
#include "render/scene_renderer.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/resource_allocator.hpp"
#include "render/visualizers/visualizer_type.hpp"

static bool g_MouseJustPressed[5] = {false, false, false, false, false};

#if defined(_WIN32)
#include <GLFW/glfw3.h>

static GLFWmousebuttonfun prev_mouse_button_callback;
static GLFWscrollfun prev_scroll_callback;
static GLFWkeyfun prev_key_callback;
static GLFWcharfun prev_char_callback;

std::array<GLFWcursor*, ImGuiMouseCursor_COUNT> mouse_cursors = {};

static const char* get_clipboard_text(void* user_data) {
    return glfwGetClipboardString(static_cast<GLFWwindow*>(user_data));
}

static void set_clipboard_text(void* user_data, const char* text) {
    glfwSetClipboardString(static_cast<GLFWwindow*>(user_data), text);
}

void mouse_button_callback(GLFWwindow* window, const int button, const int action, const int mods) {
    if (prev_mouse_button_callback != nullptr) {
        prev_mouse_button_callback(window, button, action, mods);
    }

    if (action == GLFW_PRESS && button >= 0 && button < IM_ARRAYSIZE(g_MouseJustPressed)) {
        g_MouseJustPressed[button] = true;
    }
}

void scroll_callback(GLFWwindow* window, const double x_offset, const double y_offset) {
    if (prev_scroll_callback != nullptr) {
        prev_scroll_callback(window, x_offset, y_offset);
    }

    auto& io = ImGui::GetIO();
    io.MouseWheelH += static_cast<float>(x_offset);
    io.MouseWheel += static_cast<float>(y_offset);
}

void key_callback(GLFWwindow* window, const int key, const int scancode, const int action, const int mods) {
    if (prev_key_callback != nullptr) {
        prev_key_callback(window, key, scancode, action, mods);
    }

    auto& io = ImGui::GetIO();
    if (action == GLFW_PRESS) {
        io.KeysDown[key] = true;
    }
    if (action == GLFW_RELEASE) {
        io.KeysDown[key] = false;
    }

    // Modifiers are not reliable across systems
    io.KeyMods = ImGuiModFlags_None;

    io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
    io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
    io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
    io.KeySuper = false;

    if (io.KeyCtrl) {
        io.KeyMods |= ImGuiModFlags_Ctrl;
    }
    if (io.KeyShift) {
        io.KeyMods |= ImGuiModFlags_Shift;
    }
    if (io.KeyAlt) {
        io.KeyMods |= ImGuiModFlags_Alt;
    }
}

void char_callback(GLFWwindow* window, const unsigned int c) {
    if (prev_char_callback != nullptr) {
        prev_char_callback(window, c);
    }

    auto& io = ImGui::GetIO();
    io.AddInputCharacter(c);
}
#endif

DebugUI::DebugUI(SceneRenderer& renderer_in) : renderer{renderer_in},
                                               allocator{renderer_in.get_backend().get_global_allocator()} {
    ImGui::CreateContext();

#if defined(_WIN32)
    auto& system_interface = reinterpret_cast<Win32SystemInterface&>(SystemInterface::get());
    window = system_interface.get_glfw_window();
#endif

    ImGui::CreateContext();

    auto& io = ImGui::GetIO();
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendPlatformName = "Sanity Engine";

    io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;

    io.SetClipboardTextFn = set_clipboard_text;
    io.GetClipboardTextFn = get_clipboard_text;
    io.ClipboardUserData = window;

    io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
    io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
    io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
    io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
    io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
    io.KeyMap[ImGuiKey_Insert] = GLFW_KEY_INSERT;
    io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
    io.KeyMap[ImGuiKey_Space] = GLFW_KEY_SPACE;
    io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
    io.KeyMap[ImGuiKey_KeyPadEnter] = GLFW_KEY_KP_ENTER;
    io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
    io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
    io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
    io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
    io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
    io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

#if defined(_WIN32)
    mouse_cursors[ImGuiMouseCursor_Arrow] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    mouse_cursors[ImGuiMouseCursor_TextInput] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    mouse_cursors[ImGuiMouseCursor_ResizeNS] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
    mouse_cursors[ImGuiMouseCursor_ResizeEW] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    mouse_cursors[ImGuiMouseCursor_Hand] = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    mouse_cursors[ImGuiMouseCursor_ResizeAll] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    mouse_cursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    mouse_cursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    mouse_cursors[ImGuiMouseCursor_NotAllowed] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);

    prev_mouse_button_callback = glfwSetMouseButtonCallback(window, mouse_button_callback);
    prev_scroll_callback = glfwSetScrollCallback(window, scroll_callback);
    prev_key_callback = glfwSetKeyCallback(window, key_callback);
    prev_char_callback = glfwSetCharCallback(window, char_callback);
#endif

    create_font_texture();
}

void DebugUI::draw() {
    auto& io = ImGui::GetIO();
    IM_ASSERT(
        io.Fonts->IsBuilt() &&
        "Font atlas not built! It is generally built by the renderer back-end. Missing call to renderer _NewFrame() function? e.g. ImGui_ImplOpenGL3_NewFrame()."
    );

    // Setup display size (every frame to accommodate for window resizing)
    int w, h;
    int display_w, display_h;
#if defined(_WIN32)
    glfwGetWindowSize(window, &w, &h);
    glfwGetFramebufferSize(window, &display_w, &display_h);
#endif
    io.DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));
    if (w > 0 && h > 0) {
        io.DisplayFramebufferScale = ImVec2(
            static_cast<float>(display_w) / static_cast<float>(w),
            static_cast<float>(display_h) / static_cast<float>(h)
        );
    }

    // Setup time step
    const auto current_time = glfwGetTime();
    io.DeltaTime = last_start_time > 0.0 ? static_cast<float>(current_time - last_start_time) : 1.0f / 60.0f;
    last_start_time = current_time;

#if defined(_WIN32)
    update_mouse_pos_and_buttons();
    update_mouse_cursor();
#endif

    ImGui::NewFrame();

    ImGui::ShowDemoWindow(&imgui_demo_open);

    draw_debug_menu();

    ImGui::Render();
}


void DebugUI::create_font_texture() {
    ZoneScoped;
    const auto& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);

    font_atlas_handle = allocator.create_texture(
        "Dear ImGUI Font Atlas", VK_FORMAT_R8_UNORM, {width, height}, 1, TextureUsage::StaticImage
    );

    auto& backend = renderer.get_backend();
    auto& upload_queue = backend.get_upload_queue();
    upload_queue.enqueue(
        TextureUploadJob{
            .destination = font_atlas_handle, .mip = 0,
            .data = std::vector(pixels, pixels + static_cast<ptrdiff_t>(width * height))
        }
    );

    font_atlas_descriptor_set = *vkutil::DescriptorBuilder::begin(
                                     backend, backend.get_persistent_descriptor_allocator()
                                 )
                                 .bind_image(
                                     0, {
                                         .sampler = backend.get_default_sampler(),
                                         .image = font_atlas_handle,
                                         .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                     }, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                     VK_SHADER_STAGE_FRAGMENT_BIT
                                 )
                                 .build();

    io.Fonts->TexID = reinterpret_cast<ImTextureID>(font_atlas_descriptor_set);
}

#if defined(_WIN32)
void DebugUI::update_mouse_pos_and_buttons() const {
    // Update buttons
    auto& io = ImGui::GetIO();
    for (auto i = 0; i < IM_ARRAYSIZE(io.MouseDown); i++) {
        // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are
        // shorter than 1 frame.
        io.MouseDown[i] = g_MouseJustPressed[i] || glfwGetMouseButton(window, i) != 0;
        g_MouseJustPressed[i] = false;
    }

    // Update mouse position
    const auto mouse_pos_backup = io.MousePos;
    io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);

    const auto focused = glfwGetWindowAttrib(window, GLFW_FOCUSED) != 0;
    if (focused) {
        if (io.WantSetMousePos) {
            glfwSetCursorPos(window, static_cast<double>(mouse_pos_backup.x), static_cast<double>(mouse_pos_backup.y));
        } else {
            double mouse_x, mouse_y;
            glfwGetCursorPos(window, &mouse_x, &mouse_y);
            io.MousePos = ImVec2(static_cast<float>(mouse_x), static_cast<float>(mouse_y));
        }
    }
}

void DebugUI::update_mouse_cursor() const {
    auto& io = ImGui::GetIO();
    if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) || glfwGetInputMode(window, GLFW_CURSOR) ==
        GLFW_CURSOR_DISABLED) {
        return;
    }

    const auto imgui_cursor = ImGui::GetMouseCursor();
    if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor) {
        // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    } else {
        // Show OS mouse cursor
        glfwSetCursor(
            window, mouse_cursors[imgui_cursor] ? mouse_cursors[imgui_cursor] : mouse_cursors[ImGuiMouseCursor_Arrow]
        );
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}
#endif

void DebugUI::draw_debug_menu() {
    if (ImGui::Begin("Debug", &is_debug_menu_open)) {
        if (ImGui::CollapsingHeader("Visualizers")) {
            for (auto visualizer : magic_enum::enum_values<RenderVisualization>()) {
                const auto name = magic_enum::enum_name(visualizer);
                const auto name_str = std::string{name}; // This is so sad
                if (ImGui::Selectable(name_str.c_str(), selected_visualizer == visualizer)) {
                    selected_visualizer = visualizer;
                }
            }

            renderer.set_active_visualizer(selected_visualizer);
        }

        if (ImGui::CollapsingHeader("cvars")) {
            auto cvars = CVarSystem::Get();
            cvars->DrawImguiEditor();
        }

        ImGui::End();
    }
}
