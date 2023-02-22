#pragma once

#include <cstdint>

/**
 * Number of frames that exist. We record one frame while the GPU executes the previous frame
 */
constexpr static uint32_t num_in_flight_frames = 2;
