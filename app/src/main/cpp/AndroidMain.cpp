// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>

#include <android/log.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

#include "core/application.hpp"
#include "core/system_interface.hpp"

std::unique_ptr<Application> application;

// Process the next main command.
void handle_cmd(android_app *app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            // The window is being shown, get it ready.
            if(application == nullptr) {
                SystemInterface::initialize(app);
                application = std::make_unique<Application>();
                // application->load_scene("Sponza/Sponza.compressed.glb");
                application->load_scene("deccerballs/scene.compressed.glb");
                application->update_resolution();
            }
            break;
        case APP_CMD_WINDOW_RESIZED:
            if(application != nullptr) {
                application->update_resolution();
            }
            break;
        case APP_CMD_TERM_WINDOW:
            // The window is being hidden or closed, clean it up.
            application = nullptr;
            break;
        default:
            __android_log_print(ANDROID_LOG_INFO, "SahRenderer", "event not handled: %d", cmd);
    }
}

void android_main(struct android_app *app) {

    // Set the callback to process system events
    app->onAppCmd = handle_cmd;

    // Used to poll the events in the main loop
    int events;
    android_poll_source *source;

    // Main loop
    do {
        int result;
        do {
            if ((result = ALooper_pollOnce(0, nullptr, &events, (void **) &source)) >= 0) {
                if (source != nullptr) {
                    source->process(app, source);
                }
            }
        } while (result == ALOOPER_POLL_CALLBACK);

        if(application) {
            application->tick();
        }
    } while (app->destroyRequested == 0);
}
