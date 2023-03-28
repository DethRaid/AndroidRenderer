#include <spdlog/logger.h>
#include <spdlog/sinks/android_sink.h>
#include <spdlog/spdlog.h>

#include "application.hpp"

#include <magic_enum.hpp>
#include <tracy/Tracy.hpp>

#include "system_interface.hpp"
#include "gltf/gltf_model.hpp"

static std::shared_ptr<spdlog::logger> logger;

Application::Application() : parser{fastgltf::Extensions::KHR_texture_basisu} {
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

    fastgltf::GltfDataBuffer data;
    std::unique_ptr<fastgltf::glTF> gltf;
    {
        ZoneScopedN("Parse glTF");
        data.loadFromFile(scene_path);
        if (scene_path.extension() == ".gltf") {
            gltf = parser.loadGLTF(&data, scene_path.parent_path(), fastgltf::Options::LoadExternalBuffers);
        } else if (scene_path.extension() == ".glb") {
            gltf = parser.loadBinaryGLTF(&data, scene_path.parent_path(), fastgltf::Options::LoadGLBBuffers);
        }
    }
    if (parser.getError() != fastgltf::Error::None) {
        logger->error("Could not load scene {}: {}", scene_path.string(), magic_enum::enum_name(parser.getError()));
        return;
    }

    const auto parse_error = gltf->parse(fastgltf::Category::All & ~(fastgltf::Category::Images));
    if (parse_error != fastgltf::Error::None) {
        logger->error("Could not parse glTF file {}: {}", scene_path.string(), magic_enum::enum_name(parse_error));
        return;
    }

    logger->info("Beginning import of scene {}", scene_path.string());

    auto imported_model = GltfModel{scene_path, gltf->getParsedAsset(), *scene_renderer};
    scene->add_model(imported_model);

    logger->info("Loaded scene {}", scene_path.string());
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
    delta_time = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(last_frame_duration).count())
        / 1000000.0;
    last_frame_start_time = frame_start_time;
}
