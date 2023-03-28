#pragma once

#include <filesystem>

#include <fastgltf_parser.hpp>
#include "input/input_manager.hpp"
#include "render/scene_renderer.hpp"
#include "render/render_scene.hpp"

class SystemInterface;

class Application {
public:
    explicit Application();

    void load_scene(const std::filesystem::path& scene_path);

    /**
     * Reads the window resolution from the system interface, and updates the renderer for the new resolution
     */
    void update_resolution();

    void tick();

    void update_player_location(const glm::vec3& movement_axis);

private:
    const float player_movement_speed = 2.f;

    double delta_time = 0.0;

    std::chrono::high_resolution_clock::time_point last_frame_start_time;

    std::unique_ptr<SceneRenderer> scene_renderer;

    std::unique_ptr<RenderScene> scene;

    InputManager input;

    fastgltf::Parser parser;

    void update_delta_time();
};
