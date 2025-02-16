#pragma once

/**
 * \brief Quality of the in-application lighting
 *
 * Controls the entire lighting environment: Indirect lighting, point light accuracy, mesh light accuracy, even directional light
 */
enum class LightingMethod {
    /**
     * \brief LPV indirect illumination and mesh lights. Suitable for low-end devices
     */
    LightPropagationVolume,

    /**
     * \brief Surfel-based GI
     */
    SurfelGI,
};
