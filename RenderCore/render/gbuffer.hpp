#pragma once

#include "render/backend/handles.hpp"

struct GBuffer {
    TextureHandle color;
    TextureHandle normal;
    TextureHandle data;
    TextureHandle emission;
    TextureHandle depth;
};
