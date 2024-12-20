#include "procedural_sky.hpp"

#include <tracy/Tracy.hpp>

#include "backend/pipeline_cache.hpp"
#include "backend/render_backend.hpp"
#include "render/backend/render_graph.hpp"

ProceduralSky::ProceduralSky() {
    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();

    transmittance_lut = allocator.create_texture(
        "Transmittance LUT",
        VK_FORMAT_R16G16B16A16_SFLOAT,
        glm::uvec2{256, 64},
        1,
        TextureUsage::StorageImage
    );

    multiscattering_lut = allocator.create_texture(
        "Multiscattering LUT",
        VK_FORMAT_R16G16B16A16_SFLOAT,
        glm::uvec2{32, 32},
        1,
        TextureUsage::StorageImage
    );

    sky_view_lut = allocator.create_texture(
        "Sky view LUT",
        VK_FORMAT_R16G16B16A16_SFLOAT,
        glm::uvec2{200, 200},
        1,
        TextureUsage::StorageImage
    );

    auto& pipelines = backend.get_pipeline_cache();

    transmittance_lut_pso = pipelines.create_pipeline("shaders/sky/transmittance.comp.spv");

    multiscattering_lut_pso = pipelines.create_pipeline("shaders/sky/multiscattering_lut.comp.spv");

    sky_view_lut_pso = pipelines.create_pipeline("shaders/sky/sky_view_lut.comp.spv");

    sky_application_pso = backend.begin_building_pipeline("Hillaire Sky")
                                 .set_vertex_shader("shaders/common/fullscreen.vert.spv")
                                 .set_fragment_shader("shaders/sky/hillaire.frag.spv")
                                 .set_depth_state({.enable_depth_write = false})
                                 .build();
}

void ProceduralSky::update_sky_luts(
    RenderGraph& graph,
    BufferHandle view_buffer
) const {
    auto& backend = RenderBackend::get();
    auto& descriptors = backend.get_transient_descriptor_allocator();

    {
        const auto set = descriptors.build_set(transmittance_lut_pso, 0)
                                    .bind(0, transmittance_lut)
                                    .build();

        graph.add_compute_dispatch(
            ComputeDispatch<uint32_t>{
                .name = "Generate transmittance LUT",
                .descriptor_sets = {set},
                .num_workgroups = glm::uvec3{256 / 8, 64 / 8, 1},
                .compute_shader = transmittance_lut_pso
            });
    }

    {
        const auto set = descriptors.build_set(multiscattering_lut_pso, 0)
                                    .bind(0, transmittance_lut)
                                    .bind(1, multiscattering_lut)
                                    .build();

        graph.add_compute_dispatch(
            ComputeDispatch<uint32_t>{
                .name = "Generate multiscattering LUT",
                .descriptor_sets = {set},
                .num_workgroups = glm::uvec3{32 / 8, 32 / 8, 1},
                .compute_shader = multiscattering_lut_pso
            });
    }

    {
        const auto set = descriptors.build_set(sky_view_lut_pso, 0)
                                    .bind(0, transmittance_lut)
                                    .bind(1, multiscattering_lut)
                                    .bind(2, sky_view_lut)
                                    .build();

        graph.add_compute_dispatch(
            ComputeDispatch<uint32_t>{
                .name = "Compute sky view LUT",
                .descriptor_sets = {set},
                .num_workgroups = glm::uvec3{(200 / 8) + 1, (200 / 8) + 1, 1},
                .compute_shader = sky_view_lut_pso
            });
    }
}

void ProceduralSky::render_sky(CommandBuffer& commands, const BufferHandle view_buffer) const {
    auto& backend = RenderBackend::get();

    const auto set = backend.get_transient_descriptor_allocator().build_set(sky_application_pso, 0)
                            .bind(0, transmittance_lut)
                            .bind(1, sky_view_lut)
                            .bind(2, view_buffer)
                            .build();

    commands.bind_pipeline(sky_application_pso);

    commands.bind_descriptor_set(0, set);

TODO:
    Bind the LUTs and relevanet buffers, and also make a way to get barrier information out of
    this
    class
        commands
    .
    draw(3);
}

TextureHandle ProceduralSky::get_sky_view_lut() const {
    return sky_view_lut;
}

TextureHandle ProceduralSky::get_transmission_lut() const {
    return transmittance_lut;
}
