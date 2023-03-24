#include "system_interface.hpp"

#if _WIN32

#include <fstream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "input/input_manager.hpp"

void on_glfw_key(GLFWwindow* window, const int key, const int scancode, const int action, const int mods) {
    auto* win32_system_interface = static_cast<Win32SystemInterface*>(glfwGetWindowUserPointer(window));

    if(key == GLFW_KEY_W) {
        if (action == GLFW_PRESS) {
            win32_system_interface->set_forward_axis(1.f);
        } else if(action == GLFW_RELEASE) {
            win32_system_interface->set_forward_axis(0.f);
        }
    } else if(key == GLFW_KEY_S) {
        if (action == GLFW_PRESS) {
            win32_system_interface->set_forward_axis(-1.f);
        } else if (action == GLFW_RELEASE) {
            win32_system_interface->set_forward_axis(0.f);
        }
    }

    if (key == GLFW_KEY_A) {
        if (action == GLFW_PRESS) {
            win32_system_interface->set_right_axis(1.f);
        } else if (action == GLFW_RELEASE) {
            win32_system_interface->set_right_axis(0.f);
        }
    } else if (key == GLFW_KEY_D) {
        if (action == GLFW_PRESS) {
            win32_system_interface->set_right_axis(-1.f);
        } else if (action == GLFW_RELEASE) {
            win32_system_interface->set_right_axis(0.f);
        }
    }

    if (key == GLFW_KEY_SPACE) {
        if (action == GLFW_PRESS) {
            win32_system_interface->set_up_axis(1.f);
        } else if (action == GLFW_RELEASE) {
            win32_system_interface->set_up_axis(0.f);
        }
    } else if (key == GLFW_KEY_LEFT_CONTROL) {
        if (action == GLFW_PRESS) {
            win32_system_interface->set_up_axis(-1.f);
        } else if (action == GLFW_RELEASE) {
            win32_system_interface->set_up_axis(0.f);
        }
    }
}

Win32SystemInterface::Win32SystemInterface(GLFWwindow* window_in) : window{ window_in } {
    hwnd = glfwGetWin32Window(window);

    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, on_glfw_key);
}

std::shared_ptr<spdlog::logger> Win32SystemInterface::get_logger(const std::string& name) {
    return spdlog::stdout_color_mt(name);
}

tl::optional<std::vector<uint8_t>> Win32SystemInterface::load_file(const std::filesystem::path& filepath) {
    std::ifstream file{ filepath, std::ios::binary };

    if (!file.is_open()) {
        spdlog::error("Could not open file {}", filepath.string());
        return tl::nullopt;
    }

    // get its size:
    file.seekg(0, std::ios::end);
    const auto file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    // read the data:
    std::vector<uint8_t> file_data(file_size);
    file.read(reinterpret_cast<char*>(file_data.data()), file_size);

    return file_data;
}

void Win32SystemInterface::write_file(const std::filesystem::path& filepath, const void* data, const uint32_t data_size) {
    auto file = std::ofstream{ filepath, std::ios::binary };

    if (!file.is_open()) {
        spdlog::error("Could not open file {} for writing", filepath.string());
        return;
    }

    file.write(static_cast<const char*>(data), data_size);
}

void Win32SystemInterface::poll_input(InputManager& input) {
    input.set_player_movement(raw_player_movement_axis);
}

glm::uvec2 Win32SystemInterface::get_resolution() {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    return glm::uvec2{ width, height };
}

HWND Win32SystemInterface::get_hwnd() const { return hwnd; }

HINSTANCE Win32SystemInterface::get_hinstance() const { return hinstance; }

void Win32SystemInterface::set_forward_axis(const float value) {
    raw_player_movement_axis.z = value;
}

void Win32SystemInterface::set_right_axis(const float value) {
    raw_player_movement_axis.x = value;
}

void Win32SystemInterface::set_up_axis(const float value) {
    raw_player_movement_axis.y = value;
}

#endif
