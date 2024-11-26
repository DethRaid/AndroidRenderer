#pragma once

class ResourceAllocator;
class RenderBackend;

static inline RenderBackend* g_render_backend = nullptr;
static inline ResourceAllocator* g_global_allocator = nullptr;
