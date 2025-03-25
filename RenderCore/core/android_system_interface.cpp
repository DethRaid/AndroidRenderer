#include "core/system_interface.hpp"

#if defined(__ANDROID__)

#include <cstdio>
#include <unistd.h>
#include <android/log.h>
#include <__threading_support>

#include <spdlog/sinks/android_sink.h>

static bool begin_stdout_redirection(const char* app_name);

AndroidSystemInterface::AndroidSystemInterface(android_app* app) :
    app{app}, asset_manager{app->activity->assetManager}, window{app->window} {
    begin_stdout_redirection("SAH");
}

static eastl::vector<std::shared_ptr<spdlog::logger>> all_loggers{};

std::shared_ptr<spdlog::logger> AndroidSystemInterface::get_logger(const std::string& name) {
    auto logger = spdlog::android_logger_mt(name);

    all_loggers.push_back(logger);

    return logger;
}

void AndroidSystemInterface::flush_all_loggers() {
    for (auto& log: all_loggers) {
        log->flush();
    }
}

tl::optional<eastl::vector<uint8_t>> AndroidSystemInterface::load_file(const std::filesystem::path& filepath) {
    const auto filename_string = filepath.string();

    AAsset* file = AAssetManager_open(asset_manager, filename_string.c_str(), AASSET_MODE_BUFFER);
    if (file == nullptr) {
        return tl::nullopt;
    }

    const auto file_length = AAsset_getLength(file);

    auto file_content = eastl::vector<uint8_t>{};
    file_content.resize(file_length);

    AAsset_read(file, file_content.data(), file_length);
    AAsset_close(file);

    return file_content;
}

ANativeWindow* AndroidSystemInterface::get_window() {
    return window;
}

glm::uvec2 AndroidSystemInterface::get_resolution() {
    return glm::uvec2{ANativeWindow_getHeight(window), ANativeWindow_getWidth(window)};
}

void AndroidSystemInterface::write_file(const std::filesystem::path& filepath, const void* data, uint32_t data_size) {
    FILE* file = fopen(filepath.c_str(), "w+");

    if (file != NULL)
    {
        fwrite(data, sizeof(uint8_t), data_size, file);
        fflush(file);
        fclose(file);
    }
}

void AndroidSystemInterface::poll_input(InputManager& input) {
    // TODO
}

android_app* AndroidSystemInterface::get_app() const {
    return app;
}

AAssetManager* AndroidSystemInterface::get_asset_manager() {
    return asset_manager;
}

std::string AndroidSystemInterface::get_native_library_dir() const {
    JNIEnv* env = nullptr;
    app->activity->vm->AttachCurrentThread(&env, nullptr);

    jclass contextClassDef = env->GetObjectClass(app->activity->javaGameActivity);
    const jmethodID getApplicationContextMethod =
        env->GetMethodID(contextClassDef, "getApplicationContext", "()Landroid/content/Context;");
    const jmethodID getApplicationInfoMethod = env->GetMethodID(
        contextClassDef, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
    jobject contextObject =
        env->CallObjectMethod(app->activity->javaGameActivity, getApplicationContextMethod);
    jobject applicationInfoObject = env->CallObjectMethod(contextObject, getApplicationInfoMethod);
    jclass applicationInfoObjectDef = env->GetObjectClass(applicationInfoObject);
    const jfieldID nativeLibraryDirField =
        env->GetFieldID(applicationInfoObjectDef, "nativeLibraryDir", "Ljava/lang/String;");

    jstring nativeLibraryDirJStr =
        (jstring) env->GetObjectField(applicationInfoObject, nativeLibraryDirField);
    const char* textCStr = env->GetStringUTFChars(nativeLibraryDirJStr, nullptr);
    const std::string libDir = textCStr;
    env->ReleaseStringUTFChars(nativeLibraryDirJStr, textCStr);

    env->DeleteLocalRef(nativeLibraryDirJStr);
    env->DeleteLocalRef(applicationInfoObjectDef);
    env->DeleteLocalRef(applicationInfoObject);
    env->DeleteLocalRef(contextObject);
    env->DeleteLocalRef(contextClassDef);

    app->activity->vm->DetachCurrentThread();
    return libDir;
}

// from https://codelab.wordpress.com/2014/11/03/how-to-use-standard-output-streams-for-logging-in-android-apps/

static int pfd[2];
static pthread_t thr;
static const char* tag = "myapp";

static void* thread_func(void* dummy) {
    ssize_t rdsz;
    char buf[2048];
    while ((rdsz = read(pfd[0], buf, sizeof buf - 1)) > 0) {
        if (buf[rdsz - 1] == '\n') --rdsz;
        buf[rdsz] = 0;  /* add null-terminator */
        __android_log_write(ANDROID_LOG_DEBUG, tag, buf);
    }

    return nullptr;
}

bool begin_stdout_redirection(const char* app_name) {
    tag = app_name;

    /* make stdout line-buffered and stderr unbuffered */
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    /* create the pipe and redirect stdout and stderr */
    pipe(pfd);
    dup2(pfd[1], 1);
    dup2(pfd[1], 2);

    /* spawn the logging thread */
    if (pthread_create(&thr, 0, thread_func, 0) == -1) {
        return false;
    }
    pthread_detach(thr);
    return true;
}

#endif
