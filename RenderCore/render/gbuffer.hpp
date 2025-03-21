#pragma once

#include "render/backend/handles.hpp"

struct GBuffer {
    TextureHandle color = nullptr;
    TextureHandle normals = nullptr;
    TextureHandle data = nullptr;
    TextureHandle emission = nullptr;
    TextureHandle depth = nullptr;
};
