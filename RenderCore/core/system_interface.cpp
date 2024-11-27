#include "core/system_interface.hpp"

static SystemInterface* instance;

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
