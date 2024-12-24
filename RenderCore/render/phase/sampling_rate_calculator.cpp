#include "sampling_rate_calculator.hpp"
#include <glm/geometric.hpp>

#include <tracy/Tracy.hpp>

#include "render/backend/gpu_texture.hpp"
#include "render/backend/pipeline_cache.hpp"
#include "render/backend/render_backend.hpp"

VRSAA::VRSAA() {
    auto& pipelines = RenderBackend::get().get_pipeline_cache();
    generate_shading_rate_image_shader = pipelines.create_pipeline("shaders/vrsaa/generate_shading_rate_image.comp.spv");
    contrast_shader = pipelines.create_pipeline("shaders/util/contrast_detection.comp.spv");

    sampler = RenderBackend::get().get_global_allocator().get_sampler(
        {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        });
}

void VRSAA::init(const glm::uvec2& resolution) {
    create_contrast_image(resolution);
    create_shading_rate_image(resolution);
    create_params_buffer();
}

void VRSAA::generate_shading_rate_image(RenderGraph & graph) const {
    ZoneScoped;

    const auto resolution = glm::vec2{
        shading_rate_image->create_info.extent.width, shading_rate_image->create_info.extent.height
    };
    const auto set = RenderBackend::get().get_transient_descriptor_allocator()
                                         .build_set(generate_shading_rate_image_shader, 0)
                                         .bind(0, contrast_image)
                                         .bind(1, shading_rate_image)
                                         .bind(2, params_buffer)
                                         .build();

    graph.add_compute_dispatch(
        ComputeDispatch<glm::vec2>{
            .name = "Calculate shading rate",
            .descriptor_sets = {set},
            .num_workgroups = glm::uvec3{glm::uvec2{resolution + glm::vec2{7}} / glm::uvec2{8}, 1},
            .compute_shader = generate_shading_rate_image_shader
        });

}

void VRSAA::measure_aliasing(RenderGraph& graph, const TextureHandle lit_scene) const {
    ZoneScoped;

    const auto set = RenderBackend::get().get_transient_descriptor_allocator().build_set(contrast_shader, 0)
                                         .bind(0, lit_scene, sampler)
                                         .bind(1, contrast_image)
                                         .build();

    const auto resolution = glm::vec2{lit_scene->create_info.extent.width, lit_scene->create_info.extent.height};

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

void VRSAA::create_shading_rate_image(const glm::vec2& resolution) {
    auto& allocator = RenderBackend::get().get_global_allocator();
    if(shading_rate_image != nullptr) {
        allocator.destroy_texture(shading_rate_image);
    }

    const auto max_texel_size = RenderBackend::get().get_max_shading_rate_texel_size();
    if(glm::length(max_texel_size) < 1) {
        spdlog::error("Max shading rate texel size is 0!");
        return;
    }

    const auto shading_rate_image_size = resolution / max_texel_size;

    shading_rate_image = allocator.create_texture(
        "Shading rate",
        VK_FORMAT_R8_UINT,
        shading_rate_image_size,
        1,
        TextureUsage::ShadingRateImage);
}

struct ShadingRateParams {
    glm::uvec2 contrast_image_resolution;
    glm::uvec2 shading_rate_image_resolution;
    glm::uvec2 max_rate;
    uint32_t num_shading_rates;
    glm::uvec2 rates[8];
};

void VRSAA::create_params_buffer() {
    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();
    params_buffer = RenderBackend::get().get_global_allocator().create_buffer(
        "Shading Rate Params",
        sizeof(ShadingRateParams),
        BufferUsage::StagingBuffer);

    const auto contrast_image_extent = contrast_image->create_info.extent;
    const auto shading_rate_image_extent = shading_rate_image->create_info.extent;

    // Intentional copy
    auto shading_rates = backend.get_shading_rates();
    assert(shading_rates.size() < 8);
    auto max = glm::uvec2{};
    for(const auto& rate : shading_rates) {
        max.x = std::max(max.x, rate.x);
        max.y = std::max(max.y, rate.y);
    }
    const auto num_shading_rates = static_cast<uint32_t>(shading_rates.size());
    if(shading_rates.size() < 8) {
        shading_rates.resize(8);
    }

    auto* mapped_params = allocator.map_buffer<ShadingRateParams>(params_buffer);
    *mapped_params = ShadingRateParams{
        .contrast_image_resolution = {contrast_image_extent.width, contrast_image_extent.height},
        .shading_rate_image_resolution = {shading_rate_image_extent.width, shading_rate_image_extent.height},
        .max_rate = max,
        .num_shading_rates = num_shading_rates,
        .rates = {
            shading_rates[0],
            shading_rates[1],
            shading_rates[2],
            shading_rates[3],
            shading_rates[4],
            shading_rates[5],
            shading_rates[6],
            shading_rates[7],
        }
    };
}
