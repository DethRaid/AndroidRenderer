#include "core/system_interface.hpp"

static std::unique_ptr<SystemInterface> instance;

#if defined(_WIN32)
void SystemInterface::initialize(GLFWwindow* window_in) {
    instance = new Win32SystemInterface{ window_in };
}
#elif defined(__ANDROID__)
void SystemInterface::initialize(android_app* app) {
    instance = std::make_unique<AndroidSystemInterface>(app);
}
#endif

SystemInterface& SystemInterface::get() {
    return *instance;
}

void SystemInterface::set_input_manager(InputManager& input_in) {
    input = &input_in;
}

bool SystemInterface::is_renderdoc_loaded() const {
    return renderdoc != nullptr;
}

RenderDocWrapper& SystemInterface::get_renderdoc() const {
    return *renderdoc;
}
