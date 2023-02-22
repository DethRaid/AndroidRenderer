#include "core/system_interface.hpp"

static std::unique_ptr<SystemInterface> instance;

#if defined(_WIN32)
void SystemInterface::initialize(GLFWwindow* window) {
    instance = std::make_unique<Win32SystemInterface>(window);
}
#elif defined(__ANDROID__)
void SystemInterface::initialize(android_app* app) {
    instance = std::make_unique<AndroidSystemInterface>(app);
}
#endif

SystemInterface& SystemInterface::get() {
    return *instance.get();
}
