#pragma once

enum class AntiAliasingType {
    None,
    VRSAA,
    XeSS,
    DLSS,
};

inline const char* to_string(const AntiAliasingType e) {
    switch(e) {
    case AntiAliasingType::None: return "None";
    case AntiAliasingType::VRSAA: return "VRSAA";
    case AntiAliasingType::XeSS: return "XeSS";
    case AntiAliasingType::DLSS: return "DLSS";
    default: return "unknown";
    }
}
