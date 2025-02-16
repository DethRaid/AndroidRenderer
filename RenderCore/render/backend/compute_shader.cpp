#include "compute_shader.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/android_sink.h>

#include "pipeline_builder.hpp"
#include "core/system_interface.hpp"
#include "graphics_pipeline.hpp"
#include "render_backend.hpp"

static std::shared_ptr<spdlog::logger> logger;
