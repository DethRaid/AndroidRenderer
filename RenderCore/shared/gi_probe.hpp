#pragma once

#include "shared/prelude.h"

/**
 * GI probe, based on page 17 of https://gdcvault.com/play/1034763/Advanced-Graphics-Summit-Raytracing-in
 */
struct GiProbe {
};

struct ProbeCascade {
    float3 min;
    float probe_spacing;
};

struct ProbeTraceResult {
    half4 irradiance;
    half4 ray_direction_and_distance;
};
