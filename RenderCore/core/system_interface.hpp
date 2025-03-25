#pragma once

#include <filesystem>
#include <memory>
#include <EASTL/vector.h>
#include <string>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/logger.h>
#include <tl/optional.hpp>

#if defined(__ANDROID__)
#include <game-activity/native_app_glue/android_native_app_glue.h>

#elif defined(_WIN32)
#include <Windows.h>
#include <GLFW/glfw3.h>

#ifdef far
#undef far
#endif
#ifdef near
#undef near
#endif
#endif

#include "render/render_doc_wrapper.hpp"

struct GLFWwindow;
class InputManager;

/**
 * Interface to the system
 */
class SystemInterface
{
public:
#if defined(_WIN32)
    static void initialize(GLFWwindow* window_in);
#elif defined(__ANDROID__)
    static void initialize(android_app* app);
#endif

    static SystemInterface& get();

    virtual ~SystemInterface() = default;

    /**
     * Gets a system logger with the specified name
     *
     * The logger may print to a file, to the system logs, to stdout, to somewhere else
     */
    virtual std::shared_ptr<spdlog::logger> get_logger(const std::string& name) = 0;

    virtual void flush_all_loggers() = 0;

    /**
     * Reads a file in its entirety
     *
     * This method returns an empty optional if the file can't be read. It returns a zero-length vector if the file can
     * be read but just happens to have no data
     */
    virtual tl::optional<eastl::vector<uint8_t>> load_file(const std::filesystem::path& filepath) = 0;

    /**
     * Writes some data to a file
     */
    virtual void write_file(const std::filesystem::path& filepath, const void* data, uint32_t data_size) = 0;

    /**
     * Polls the platform's input state and pushes it to the input manager
     */
    virtual void poll_input(InputManager& input) = 0;

    virtual glm::uvec2 get_resolution() = 0;

    void set_input_manager(InputManager& input_in);

    InputManager* input = nullptr;

    bool is_renderdoc_loaded() const;

    RenderDocWrapper& get_renderdoc() const;

    virtual std::string get_native_library_dir() const = 0;

protected:
    std::unique_ptr<RenderDocWrapper> renderdoc;
};

#if defined(__ANDROID__)
// Android implementation
/**
 * Android implementation of the system interface
 */
class AndroidSystemInterface : public SystemInterface {
public:
    AndroidSystemInterface(android_app* app);

    std::shared_ptr<spdlog::logger> get_logger(const std::string& name) override;

    void flush_all_loggers() override;

    tl::optional<eastl::vector<uint8_t>> load_file(const std::filesystem::path& filepath) override;

    void write_file(const std::filesystem::path& filepath, const void* data, uint32_t data_size) override;

    void poll_input(InputManager& input) override;

    glm::uvec2 get_resolution() override;

    std::string get_native_library_dir() const override;

    android_app* get_app() const;

    ANativeWindow* get_window();

    AAssetManager* get_asset_manager();

private:
    android_app* app = nullptr;

    AAssetManager* asset_manager = nullptr;

    ANativeWindow* window = nullptr;
};

#elif defined(_WIN32)
// Windows implementation
class Win32SystemInterface final : public SystemInterface {
public:
    explicit Win32SystemInterface(GLFWwindow* window_in);

    std::shared_ptr<spdlog::logger> get_logger(const std::string& name) override;

    void flush_all_loggers() override;

    tl::optional<eastl::vector<uint8_t>> load_file(const std::filesystem::path& filepath) override;

    void write_file(const std::filesystem::path& filepath, const void* data, uint32_t data_size) override;

    void poll_input(InputManager& input) override;

    glm::uvec2 get_resolution() override;

    std::string get_native_library_dir() const override;

    HWND get_hwnd() const;

    HINSTANCE get_hinstance() const;

    void set_forward_axis(float value);

    void set_right_axis(float value);

    void set_up_axis(float value);

    void set_cursor_position(glm::vec2 new_position);

    void set_focus(bool focused_in);

    GLFWwindow* get_glfw_window() const;

private:
    std::shared_ptr<spdlog::logger> logger;

    GLFWwindow* window = nullptr;

    HWND hwnd = nullptr;

    HINSTANCE hinstance = nullptr;

    glm::vec3 raw_player_movement_axis = glm::vec3{0};

    glm::vec2 raw_cursor_input = {};

    glm::vec2 last_cursor_position = {};

    bool focused = true;

    void init_renderdoc_api();
};
#endif
