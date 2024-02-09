#pragma once

/**
 * \brief Quality of the in-application lighting
 *
 * Controls the entire lighting environment: Indirect lighting, point light accuracy, mesh light accuracy, even directional light
 */
enum class LightingQuality {
    /**
     * \brief LPV indirect illumination, LPV mesh lights, analytical directional light
     */
    Low,

    /**
     * \brief Raytraced one-bounce indirect illumination, raytraced mesh lights, raytraced directional light
     */
    High,
};
