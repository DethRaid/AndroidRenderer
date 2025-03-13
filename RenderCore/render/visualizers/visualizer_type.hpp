#pragma once

enum class RenderVisualization : uint8_t {
    None,
    VPLs,
};

inline const char* to_string(const RenderVisualization e) {
    switch(e) {
    case RenderVisualization::None: return "None";
    case RenderVisualization::VPLs: return "VPLs";
    default: return "unknown";
    }
}
