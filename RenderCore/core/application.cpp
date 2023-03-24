#include <spdlog/logger.h>
#include <spdlog/sinks/android_sink.h>
#include <spdlog/spdlog.h>

#include "application.hpp"

#include "system_interface.hpp"
#include "gltf/gltf_model.hpp"

static std::shared_ptr<spdlog::logger> logger;

Application::Application() {
    logger = SystemInterface::get().get_logger("Application");
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::warn);

    scene_renderer = std::make_unique<SceneRenderer>();
    scene = std::make_unique<RenderScene>(scene_renderer->get_backend());

    scene_renderer->set_scene(*scene);

    input.add_player_movement_callback([&](const glm::vec3& movement) { update_player_location(movement); });

    last_frame_start_time = std::chrono::high_resolution_clock::now();

    logger->info("HELLO HUMAN");
}

void Application::load_scene(const std::filesystem::path& scene_path) {
    logger->info("Beginning load of scene {}", scene_path.string());

    tinygltf::Model model;
    std::string err;
    std::string warn;
    auto success = false;
    {
        ZoneScopedN("Parse glTF");
        success = loader.LoadASCIIFromFile(&model, &err, &warn, scene_path.string());
        if (!warn.empty()) {
            logger->warn("Warnings generated when loading {}: {}", scene_path.string(), warn);
        }
    }
    if (success) {
        logger->info("Beginning import of scene {}", scene_path.string());

        auto imported_model = GltfModel{scene_path, model, *scene_renderer};
        scene->add_model(imported_model);

        logger->info("Loaded scene {}", scene_path.string());
    } else {
        logger->error("Could not load scene {}: {}", scene_path.string(), err);
    }
}

void Application::update_resolution() {
    const auto& new_resolution = SystemInterface::get().get_resolution();
    scene_renderer->set_render_resolution(new_resolution);
}

void Application::tick() {
    update_delta_time();

    SystemInterface::get().poll_input(input);

    input.dispatch_callbacks();

    scene_renderer->render();
}

void Application::update_player_location(const glm::vec3& movement_axis) {
    const auto movement = movement_axis * player_movement_speed * static_cast<float>(delta_time);

    scene_renderer->translate_player(movement);
}

void Application::update_delta_time() {
    const auto frame_start_time = std::chrono::high_resolution_clock::now();
    const auto last_frame_duration = frame_start_time - last_frame_start_time;
    delta_time = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(last_frame_duration).count()) / 1000000.0;
    last_frame_start_time = frame_start_time;
}
