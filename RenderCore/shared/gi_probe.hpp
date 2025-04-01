#pragma once

#include "shared/prelude.h"

struct ProbeCascade {
    float3 min;
    float probe_spacing;
};

struct IrradianceProbeVolume {
    ProbeCascade cascades[4];
    u16vec2 trace_resolution;
    u16vec2 rgti_probe_resolution;
    u16vec2 light_cache_probe_resolution;
    u16vec2 depth_probe_resolution;
};
