#pragma once

// Intentionally using C enum because I want named ints
// ReSharper disable once CppInconsistentNaming
namespace ScenePassType {
    enum Type {
        /**
         * Reflectance shadow map pass. The pipeline used for an object must output its flux, normal, and depth
         */
        RSM,

        /**
         * Regular shadow map pass. The pipeline must output depth
         */
        Shadow,

        /**
         * Gbuffer pass. The pipeline must output base color, normal, material data, and emission
         */
        Gbuffer,

        /**
         * Vozelization pass. The pipeline must position its triangle within a 3D texture, and the pipeline must output
         * spherical harmonics to represent the surface
         */
        Vozelization,

        /**
         * \brief Depth-only prepass
         */
        DepthPrepass,

        Count,
    };
}

inline bool is_color_pass(const ScenePassType::Type pass_type) {
    switch(pass_type) {
    case ScenePassType::RSM:
        [[fallthrough]];
    case ScenePassType::Gbuffer:
        return true;

    case ScenePassType::Shadow:
        [[fallthrough]];
    case ScenePassType::Vozelization:
        [[fallthrough]];
    case ScenePassType::DepthPrepass:
        return false;

    default:
        return false;
    }

    return false;
}
