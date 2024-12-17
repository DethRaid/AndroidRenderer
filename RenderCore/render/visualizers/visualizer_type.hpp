#pragma once

enum class RenderVisualization {
    None,
    VoxelizedMeshes,
    VPLs,
};

inline const char* to_string(const RenderVisualization e) {
    switch(e) {
    case RenderVisualization::None: return "None";
    case RenderVisualization::VoxelizedMeshes: return "VoxelizedMeshes";
    case RenderVisualization::VPLs: return "VPLs";
    default: return "unknown";
    }
}
