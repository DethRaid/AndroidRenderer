#include <spdlog/logger.h>
#include <spdlog/sinks/android_sink.h>

#include "application.hpp"

#include "system_interface.hpp"
#include "gltf/gltf_model.hpp"
#include "spdlog/spdlog.h"

static std::shared_ptr<spdlog::logger> logger;

Application::Application() {
    logger = SystemInterface::get().get_logger("Application");
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::warn);
        
    scene_renderer = std::make_unique<SceneRenderer>();
    scene = std::make_unique<RenderScene>(scene_renderer->get_backend());

    scene_renderer->set_scene(*scene);

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
    scene_renderer->render();
}
