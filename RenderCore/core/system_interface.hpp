#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include <glm/vec2.hpp>
#include <spdlog/logger.h>
#include <tl/optional.hpp>

#if defined(__ANDROID__)
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include "glm/fwd.hpp"

#elif defined(_WIN32)
#include <Windows.h>
#ifdef far
#undef far
#endif
#ifdef near
#undef near
#endif

struct GLFWwindow;
#endif

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

    /**
     * Reads a file in its entirety
     *
     * This method returns an empty optional if the file can't be read. It returns a zero-length vector if the file can
     * be read but just happens to have no data
     */
    virtual tl::optional<std::vector<uint8_t>> load_file(const std::filesystem::path& filepath) = 0;

    /**
     * Writes some data to a file
     */
    virtual void write_file(const std::filesystem::path& filepath, const void* data, uint32_t data_size) = 0;

    virtual glm::uvec2 get_resolution() = 0;
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

    tl::optional<std::vector<uint8_t>> load_file(const std::filesystem::path& filepath) override;

    void write_file(const std::filesystem::path& filepath, const void* data, uint32_t data_size) override;

    ANativeWindow* get_window();

    glm::uvec2 get_resolution() override;

private:
    AAssetManager* asset_manager = nullptr;

    ANativeWindow* window = nullptr;
};

#elif defined(_WIN32)
// Windows implementation
class Win32SystemInterface final : public SystemInterface {
public:
    explicit Win32SystemInterface(GLFWwindow* window_in);

    std::shared_ptr<spdlog::logger> get_logger(const std::string& name) override;

    tl::optional<std::vector<uint8_t>> load_file(const std::filesystem::path& filepath) override;

    void write_file(const std::filesystem::path& filepath, const void* data, uint32_t data_size) override;

    glm::uvec2 get_resolution() override;

    HWND get_hwnd() const;

    HINSTANCE get_hinstance() const;

private:
    GLFWwindow* window = nullptr;

    HWND hwnd = nullptr;

    HINSTANCE hinstance = nullptr;
};
#endif
