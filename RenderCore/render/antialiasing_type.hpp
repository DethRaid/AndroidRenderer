#pragma once

enum class AntiAliasingType {
    None,
    VRSAA,
    FSR3,
    DLSS,
    XeSS,
};

inline const char* to_string(const AntiAliasingType e) {
    switch(e) {
    case AntiAliasingType::None: return "None";
    case AntiAliasingType::VRSAA: return "VRSAA";
    case AntiAliasingType::FSR3: return "FSR3";
    case AntiAliasingType::DLSS: return "DLSS";
    case AntiAliasingType::XeSS: return "XeSS";
    default: return "unknown";
    }
}
