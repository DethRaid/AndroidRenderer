#include "sampling_rate_calculator.hpp"

#include <tracy/Tracy.hpp>

#include "render/backend/gpu_texture.hpp"
#include "render/backend/pipeline_cache.hpp"
#include "render/backend/render_backend.hpp"

VRSAA::VRSAA() {
    contrast_shader = RenderBackend::get().get_pipeline_cache().create_pipeline(
        "shaders/util/contrast_detection.comp.spv");

    sampler = RenderBackend::get().get_global_allocator().get_sampler(
        {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        });
}

void VRSAA::measure_aliasing(RenderGraph& graph, const TextureHandle lit_scene) {
    ZoneScoped;

    const auto resolution = glm::vec2{lit_scene->create_info.extent.width, lit_scene->create_info.extent.height};
    if(contrast_image == nullptr) {
        create_contrast_image(resolution);
    }

    const auto set = RenderBackend::get().get_transient_descriptor_allocator().build_set(contrast_shader, 0)
                                         .bind(0, lit_scene, sampler)
                                         .bind(1, contrast_image)
                                         .build();

    graph.add_compute_dispatch(
        ComputeDispatch<glm::vec2>{
            .name = "Contrast",
            .descriptor_sets = {set},
            .push_constants = resolution,
            .num_workgroups = glm::uvec3{glm::uvec2{resolution + glm::vec2{7}} / glm::uvec2{8}, 1},
            .compute_shader = contrast_shader
        });
}

void VRSAA::create_contrast_image(const glm::vec2& resolution) {
    auto& allocator = RenderBackend::get().get_global_allocator();

    if(contrast_image != nullptr) {
        allocator.destroy_texture(contrast_image);
    }

    contrast_image = allocator.create_texture(
        "Contrast",
        VK_FORMAT_R16_SFLOAT,
        resolution,
        1,
        TextureUsage::StorageImage);
}
