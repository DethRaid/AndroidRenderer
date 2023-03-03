#pragma once

enum class ScenePassType {
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
    Vozelization
};
