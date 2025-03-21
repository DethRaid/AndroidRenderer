#include "windows_application.hpp"

#include <core/system_interface.hpp>
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <core/application.hpp>
#include <tracy/Tracy.hpp>

int main(const int argc, const char** argv) {
    //try {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        GLFWwindow* window = glfwCreateWindow(1920, 1080, "SAH Renderer", nullptr, nullptr);

        SystemInterface::initialize(window);

        Application application;

        {
            ZoneScopedN("Init application");
            //application.load_scene("assets/Sponza/Sponza.compressed.glb");
            // application.load_scene("assets/path_tracing_nightmare.compressed.glb");
            // application.load_scene("assets/shadow_test.glb");
            // application.load_scene("assets/deccercube/SM_Deccer_Cubes_Textured.compressed.glb");
            application.load_scene("assets/Bistro_v5_2/BistroExterior.compressed.glb");
            // application.load_scene("assets/Main.1_Sponza/NewSponza_Main_glTF_002.compressed.glb");
            // application.load_scene("assets/PKG_A_Curtains/NewSponza_Curtains_glTF.compressed.glb");
            // application.load_scene("assets/deccerballs/scene.compressed.glb");
            // application.load_scene("assets/deccerballs/balls_with_background.compressed.glb");
            // application.load_scene("assets/SanMiguel/SanMiguel.compressed.glb");
            // application.load_scene("assets/Small_City_LVL/Small_City_LVL.compressed.glb");
            // application.load_scene("assets/BoomBoxWithAxes/gltf/BoomBoxWithAxes.gltf");
            // application.load_scene("assets/NormalTangentTest/gltf/NormalTangentTest.gltf");
            // application.load_scene("assets/AlphaTest.gltf");
            //application.load_scene("assets/CornelBoxKinda.glb");
            application.update_resolution();
        }

        while (!glfwWindowShouldClose(window)) {
            ZoneScopedN("Frame");

            application.tick();
        }

        glfwTerminate();
    //} catch(const std::exception& e) {
    //    spdlog::error("Unable to execute application: {}", e.what());
    //}
}
