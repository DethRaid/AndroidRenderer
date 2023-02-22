#pragma once

#include <tiny_gltf.h>
#include <filesystem>

#include "render/scene_renderer.hpp"
#include "render/render_scene.hpp"

class SystemInterface;

class Application {
public:
    explicit Application();

    void load_scene(const std::filesystem::path& path);

    /**
     * Reads the window resolution from the system interface, and updates the renderer for the new resolution
     */
    void update_resolution();

    void tick();

private:
    std::unique_ptr<SceneRenderer> scene_renderer;

    std::unique_ptr<RenderScene> scene;

    tinygltf::TinyGLTF loader;
};
