#pragma once

enum class RenderVisualization : uint8_t {
    None,
    GIDebug,
};

inline const char* to_string(const RenderVisualization e) {
    switch(e) {
    case RenderVisualization::None: return "None";
    case RenderVisualization::GIDebug: return "Global Illumination";
    default: return "unknown";
    }
}
