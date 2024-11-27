#pragma once

#include <renderdoc/renderdoc_app.h>

#include "spdlog/logger.h"

class RenderDocWrapper {
public:
    explicit RenderDocWrapper(RENDERDOC_API_1_1_2* api_in);

    void begin_capture() const;

    void end_capture() const;

private:
    RENDERDOC_API_1_1_2* api = nullptr;
};
