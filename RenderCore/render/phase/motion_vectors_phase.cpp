#include "motion_vectors_phase.hpp"

#include "render/backend/render_backend.hpp"

MotionVectorsPhase::MotionVectorsPhase() {}

void MotionVectorsPhase::set_render_resolution(const glm::uvec2& resolution) {
    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();

    if(motion_vectors) {
        if(motion_vectors->create_info.extent.width != resolution.x || motion_vectors->create_info.extent.height !=
            resolution.y) {
            allocator.destroy_texture(motion_vectors);
            motion_vectors = nullptr;
        }
    }

    if(motion_vectors == nullptr) {
        motion_vectors = allocator.create_texture(
            "motion_vectors",
            {.format = VK_FORMAT_R16G16_SFLOAT, .resolution = resolution, .usage = TextureUsage::RenderTarget}
        );
    }
}

TextureHandle MotionVectorsPhase::get_motion_vectors() const {
    return motion_vectors;
}
