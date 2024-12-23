#pragma once

enum class AntiAliasingType {
    None,
    VRSAA,
};

inline const char* to_string(const AntiAliasingType e) {
    switch(e) {
    case AntiAliasingType::None: return "None";
    case AntiAliasingType::VRSAA: return "VRSAA";
    default: return "unknown";
    }
}
