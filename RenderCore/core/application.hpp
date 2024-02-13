#pragma once

#include <filesystem>

#include <fastgltf_parser.hpp>

#include "user_options_controller.hpp"
#include "input/input_manager.hpp"
#include "render/scene_renderer.hpp"
#include "render/render_scene.hpp"
#include "ui/debug_menu.hpp"

class SystemInterface;

class Application {
public:
    explicit Application();

    void load_scene(const std::filesystem::path& scene_path);

    /**
     * Reads the window resolution from the system interface, and updates the renderer for the new resolution
     */
    void update_resolution() const;

    void tick();

    void update_player_location(const glm::vec3& movement_axis) const;

    void update_player_rotation(const glm::vec2& rotation_input);

    SceneRenderer& get_renderer() const;

private:
    const float player_movement_speed = 2.f;

    const float player_rotation_speed = 0.05;

    double delta_time = 0.0;

    std::chrono::high_resolution_clock::time_point last_frame_start_time;

    std::unique_ptr<SceneRenderer> scene_renderer;

    std::unique_ptr<RenderScene> scene;
    
    InputManager input;

    fastgltf::Parser parser;

    DebugMenu debug_menu;

    void update_delta_time();
};
