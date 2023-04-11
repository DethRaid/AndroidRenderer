#include "windows_application.hpp"

#include <core/system_interface.hpp>
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <core/application.hpp>
#include <tracy/Tracy.hpp>

int main(const int argc, const char** argv) {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "SAH Renderer", nullptr, nullptr);
    
    SystemInterface::initialize(window);

    Application application;

    {
        ZoneScopedN("Init application");
        application.load_scene("Sponza/Sponza.compressed.glb");
        // application.load_scene("path_tracing_nightmare.compressed.glb");
        // application.load_scene("shadow_test.glb");
        // application.load_scene("deccercube/SM_Deccer_Cubes_Textured.compressed.glb");
        // application.load_scene("Bistro_v5_2/BistroExterior.compressed.glb");
        // application.load_scene("Main.1_Sponza/NewSponza_Main_glTF_002.compressed.glb");
        // application.load_scene("PKG_A_Curtains/NewSponza_Curtains_glTF.compressed.glb");
        application.update_resolution();
    }
        
    while (!glfwWindowShouldClose(window)) {
        ZoneScopedN("Frame");

        application.tick();

        glfwPollEvents();
    }
    
    glfwTerminate();
}
