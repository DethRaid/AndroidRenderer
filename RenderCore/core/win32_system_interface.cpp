#include "system_interface.hpp"

#if _WIN32

#include <fstream>

#include <renderdoc/renderdoc_app.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "input/input_manager.hpp"
#include "render/render_doc_wrapper.hpp"

static void on_glfw_key(GLFWwindow* window, const int key, const int scancode, const int action, const int mods) {
    auto* win32_system_interface = static_cast<Win32SystemInterface*>(glfwGetWindowUserPointer(window));

    // TODO: Find some way to generalize this and not having key bindings hardcoded into the platform layer
    // The core should define a bunch of actions, then the platform layers can define which physical inputs map to which actions

    if(key == GLFW_KEY_W) {
        if(action == GLFW_PRESS) {
            win32_system_interface->set_forward_axis(-1.f);
        } else if(action == GLFW_RELEASE) {
            win32_system_interface->set_forward_axis(0.f);
        }
    } else if(key == GLFW_KEY_S) {
        if(action == GLFW_PRESS) {
            win32_system_interface->set_forward_axis(1.f);
        } else if(action == GLFW_RELEASE) {
            win32_system_interface->set_forward_axis(0.f);
        }
    }

    if(key == GLFW_KEY_A) {
        if(action == GLFW_PRESS) {
            win32_system_interface->set_right_axis(-1.f);
        } else if(action == GLFW_RELEASE) {
            win32_system_interface->set_right_axis(0.f);
        }
    } else if(key == GLFW_KEY_D) {
        if(action == GLFW_PRESS) {
            win32_system_interface->set_right_axis(1.f);
        } else if(action == GLFW_RELEASE) {
            win32_system_interface->set_right_axis(0.f);
        }
    }

    if(key == GLFW_KEY_SPACE) {
        if(action == GLFW_PRESS) {
            win32_system_interface->set_up_axis(1.f);
        } else if(action == GLFW_RELEASE) {
            win32_system_interface->set_up_axis(0.f);
        }
    } else if(key == GLFW_KEY_LEFT_CONTROL) {
        if(action == GLFW_PRESS) {
            win32_system_interface->set_up_axis(-1.f);
        } else if(action == GLFW_RELEASE) {
            win32_system_interface->set_up_axis(0.f);
        }
    }
}

static void on_glfw_cursor(GLFWwindow* window, const double xpos, const double ypos) {
    auto* win32_system_interface = static_cast<Win32SystemInterface*>(glfwGetWindowUserPointer(window));

    win32_system_interface->set_cursor_position(glm::vec2{xpos, ypos});
}

static void on_glfw_focus(GLFWwindow* window, const int focused) {
    auto* win32_system_interface = static_cast<Win32SystemInterface*>(glfwGetWindowUserPointer(window));
    win32_system_interface->set_focus(focused);
}

static void on_glfw_mouse_button(GLFWwindow* window, const int button, const int action, const int mods) {
    auto* win32_system_interface = static_cast<Win32SystemInterface*>(glfwGetWindowUserPointer(window));
    if(button == GLFW_MOUSE_BUTTON_2) {
        if(action == GLFW_PRESS) {
            win32_system_interface->input->add_input_event(
                {.button = InputButtons::FlycamEnabled, .action = InputAction::Pressed});

        } else if(action == GLFW_RELEASE) {
            win32_system_interface->input->add_input_event(
                {.button = InputButtons::FlycamEnabled, .action = InputAction::Released});
        }
    }
}

Win32SystemInterface::Win32SystemInterface(GLFWwindow* window_in) : window{window_in} {
    logger = get_logger("Win32SystemInterface");

    hwnd = glfwGetWin32Window(window);

    // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, on_glfw_key);
    glfwSetCursorPosCallback(window, on_glfw_cursor);
    glfwSetWindowFocusCallback(window, on_glfw_focus);
    glfwSetMouseButtonCallback(window, on_glfw_mouse_button);

    auto window_size = glm::ivec2{};
    glfwGetWindowSize(window, &window_size.x, &window_size.y);
    last_cursor_position = window_size / 2;


    init_renderdoc_api();
}

static eastl::vector<std::shared_ptr<spdlog::logger>> all_loggers{};

std::shared_ptr<spdlog::logger> Win32SystemInterface::get_logger(const std::string& name) {
    auto sinks = eastl::vector<spdlog::sink_ptr>{
        std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
        std::make_shared<spdlog::sinks::basic_file_sink_mt>("sah.log", true),
    };
    sinks[0]->set_pattern("[%n] [%^%l%$] %v");
    auto new_logger = std::make_shared<spdlog::logger>(name.c_str(), sinks.begin(), sinks.end());

#ifndef NDEBUG
    new_logger->set_level(spdlog::level::debug);
#else
    new_logger->set_level(spdlog::level::warn);
#endif

    // Register the logger so we can access it later if needed
    spdlog::register_logger(new_logger);
    all_loggers.emplace_back(new_logger);

    return new_logger;
}

void Win32SystemInterface::flush_all_loggers() {
    for(auto& log : all_loggers) {
        log->flush();
    }
}

tl::optional<eastl::vector<std::byte>> Win32SystemInterface::load_file(const std::filesystem::path& filepath) {
    // TODO: Integrate physfs and add the executable's directory to the search paths
    std::ifstream file{filepath, std::ios::binary};

    if(!file.is_open()) {
        spdlog::warn("Could not open file {}", filepath.string());
        return tl::nullopt;
    }

    // get its size:
    file.seekg(0, std::ios::end);
    const auto file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    // read the data:
    eastl::vector<std::byte> file_data(file_size);
    file.read(reinterpret_cast<char*>(file_data.data()), file_size);

    return file_data;
}

void Win32SystemInterface::write_file(
    const std::filesystem::path& filepath, const void* data, const uint32_t data_size
) {
    if(filepath.has_parent_path()) {
        std::filesystem::create_directories(filepath.parent_path());
    }

    auto file = std::ofstream{filepath, std::ios::binary};

    if(!file.is_open()) {
        spdlog::error("Could not open file {} for writing", filepath.string());
        return;
    }

    file.write(static_cast<const char*>(data), data_size);
}

void Win32SystemInterface::poll_input(InputManager& input) {
    glfwPollEvents();

    if(!focused) {
        return;
    }

    input.set_player_movement(raw_player_movement_axis);

    input.set_player_rotation(raw_cursor_input * -1.f);

    auto window_size = glm::ivec2{};
    glfwGetWindowSize(window, &window_size.x, &window_size.y);
    // const auto half_window_size = window_size / 2;
    // glfwSetCursorPos(window, half_window_size.x, half_window_size.y);
}

glm::uvec2 Win32SystemInterface::get_resolution() {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    return glm::uvec2{width, height};
}

std::string Win32SystemInterface::get_native_library_dir() const {
    return "";
}

HWND Win32SystemInterface::get_hwnd() const {
    return hwnd;
}

HINSTANCE Win32SystemInterface::get_hinstance() const {
    return hinstance;
}

void Win32SystemInterface::set_forward_axis(const float value) {
    raw_player_movement_axis.z = value;
}

void Win32SystemInterface::set_right_axis(const float value) {
    raw_player_movement_axis.x = value;
}

void Win32SystemInterface::set_up_axis(const float value) {
    raw_player_movement_axis.y = value;
}

void Win32SystemInterface::set_cursor_position(const glm::vec2 new_position) {
    raw_cursor_input = new_position - last_cursor_position;
    last_cursor_position = new_position;
}

void Win32SystemInterface::set_focus(const bool focused_in) {
    focused = focused_in;
}

GLFWwindow* Win32SystemInterface::get_glfw_window() const {
    return window;
}

void Win32SystemInterface::init_renderdoc_api() {
    if(auto mod = GetModuleHandleA("renderdoc.dll")) {
        RENDERDOC_API_1_1_2* rdoc_api = nullptr;
        auto renderdoc_get_api = reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(mod, "RENDERDOC_GetAPI"));
        int ret = renderdoc_get_api(eRENDERDOC_API_Version_1_1_2, reinterpret_cast<void**>(&rdoc_api));
        if(ret == 1) {
            renderdoc = std::make_unique<RenderDocWrapper>(rdoc_api);
            
        }
    }
}

#endif
