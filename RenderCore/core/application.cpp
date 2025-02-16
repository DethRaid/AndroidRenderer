#include <spdlog/logger.h>
#include <spdlog/sinks/android_sink.h>
#include <spdlog/spdlog.h>

#include "application.hpp"

#include <imgui.h>
#include <magic_enum.hpp>
#include <tracy/Tracy.hpp>

#include "system_interface.hpp"
#include "model_import/gltf_model.hpp"

static std::shared_ptr<spdlog::logger> logger;

Application::Application() : parser{fastgltf::Extensions::KHR_texture_basisu} {
    logger = SystemInterface::get().get_logger("Application");
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::critical);
    spdlog::flush_on(spdlog::level::err);
    spdlog::flush_on(spdlog::level::warn);

    SystemInterface::get().set_input_manager(input);

    scene_renderer = std::make_unique<SceneRenderer>();
    scene = std::make_unique<RenderScene>(
        scene_renderer->get_mesh_storage(), scene_renderer->get_material_storage()
    );

    scene_renderer->set_scene(*scene);

    input.add_input_event_callback([&](const InputEvent& event) { switch (event.button) {
    case InputButtons::FlycamEnabled:
        if(event.action == InputAction::Pressed) {
            logger->trace("Enabling the flycam");
            flycam_enabled = true;
        } else {
            logger->trace("Disabling the flycam");
            flycam_enabled = false;
        }
        break;
    } });
    input.add_player_movement_callback([&](const glm::vec3& movement) { update_player_location(movement); });
    input.add_player_rotation_callback([&](const glm::vec2& rotation) { update_player_rotation(rotation); });

    last_frame_start_time = std::chrono::high_resolution_clock::now();

    debug_menu = std::make_unique<DebugUI>(*scene_renderer);

    logger->info("HELLO HUMAN");
}

void Application::load_scene(const std::filesystem::path& scene_path) {
    ZoneScoped;
    logger->info("Beginning load of scene {}", scene_path.string());

    if(!exists(scene_path)) {
        logger->error("Scene file {} does not exist!", scene_path.string());
        return;
    }

    if (scene_path.has_parent_path()) {
        logger->info("Scene path {} has parent path {}", scene_path.string(), scene_path.parent_path().string());
    } else {
        logger->warn("Scene path {} has no parent path!", scene_path.string());
    }

#if defined(__ANDROID__)
    auto& system_interface = reinterpret_cast<AndroidSystemInterface&>(SystemInterface::get());
    auto data = fastgltf::AndroidGltfDataBuffer{system_interface.get_asset_manager()};
    {
        ZoneScopedN("Load file data");
        data.loadFromAndroidAsset(scene_path);
    }
#else
    fastgltf::GltfDataBuffer data;
    {
        ZoneScopedN("Load file data");
        data.loadFromFile(scene_path);
    }
#endif
    std::unique_ptr<fastgltf::glTF> gltf;
    {
        ZoneScopedN("Parse glTF");
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
    imported_model.add_to_scene(*scene, *scene_renderer);

    logger->info("Loaded scene {}", scene_path.string());
}

void Application::update_resolution() const {
    const auto& screen_resolution = SystemInterface::get().get_resolution();
    scene_renderer->set_render_resolution(screen_resolution);
}

void Application::tick() {
    ZoneScoped;

    update_delta_time();

    logger->debug("Tick {:.3f} ms ({:.3f} fps)", delta_time * 1000, 1 / delta_time);

    // Input

    SystemInterface::get().poll_input(input);

    input.dispatch_callbacks();

    // TODO: Gameplay

    // UI

    debug_menu->draw();

    // Rendering

    scene_renderer->set_imgui_commands(ImGui::GetDrawData());

    scene_renderer->render();

    // TODO: Would be nice if we had UI as a separate render thing... for now the "scene" renderer does it
}

void Application::update_player_location(const glm::vec3& movement_axis) const {
    if(!flycam_enabled) {
        scene_renderer->translate_player({0.f, 0.f, 0.f});
        return;
    }

    const auto movement = movement_axis * player_movement_speed * static_cast<float>(delta_time);

    scene_renderer->translate_player(movement);
}

void Application::update_player_rotation(const glm::vec2& rotation_input) const {
    if (!flycam_enabled) {
        scene_renderer->rotate_player(0.f, 0.f);
        return;
    }

    const auto rotation = rotation_input * player_rotation_speed * static_cast<float>(delta_time);

    scene_renderer->rotate_player(rotation.y, rotation.x);
}

SceneRenderer& Application::get_renderer() const { return *scene_renderer; }

void Application::update_delta_time() {
    const auto frame_start_time = std::chrono::high_resolution_clock::now();
    const auto last_frame_duration = frame_start_time - last_frame_start_time;
    delta_time = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(last_frame_duration).count())
        / 1000000.0;
    last_frame_start_time = frame_start_time;
}
