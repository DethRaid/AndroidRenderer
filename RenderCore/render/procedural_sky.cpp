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
        {
            VK_FORMAT_R16G16B16A16_SFLOAT,
            glm::uvec2{256, 64},
            1,
            TextureUsage::StorageImage
        }
    );

    multiscattering_lut = allocator.create_texture(
        "Multiscattering LUT",
        {
            VK_FORMAT_R16G16B16A16_SFLOAT,
            glm::uvec2{32, 32},
            1,
            TextureUsage::StorageImage
        }
    );

    sky_view_lut = allocator.create_texture(
        "Sky view LUT",
        {
            VK_FORMAT_R16G16B16A16_SFLOAT,
            glm::uvec2{200, 200},
            1,
            TextureUsage::StorageImage
        }
    );

    auto& pipelines = backend.get_pipeline_cache();

    transmittance_lut_pso = pipelines.create_pipeline("shaders/sky/transmittance_lut.comp.spv");

    multiscattering_lut_pso = pipelines.create_pipeline("shaders/sky/multiscattering_lut.comp.spv");

    sky_view_lut_pso = pipelines.create_pipeline("shaders/sky/sky_view_lut.comp.spv");

    sky_application_pso = backend.begin_building_pipeline("Hillaire Sky")
                                 .set_vertex_shader("shaders/common/fullscreen.vert.spv")
                                 .set_fragment_shader("shaders/sky/hillaire.frag.spv")
                                 .set_depth_state({.enable_depth_write = false})
                                 .build();

    linear_sampler = allocator.get_sampler(
        {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .maxLod = VK_LOD_CLAMP_NONE,
        });
}

void ProceduralSky::update_sky_luts(RenderGraph& graph, const glm::vec3& light_vector) const {
    auto& backend = RenderBackend::get();
    auto& descriptors = backend.get_transient_descriptor_allocator();

    graph.begin_label("Update sky LUTs");

    {
        const auto set = descriptors.build_set(transmittance_lut_pso, 0)
                                    .bind(transmittance_lut)
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
                                    .bind(transmittance_lut, linear_sampler)
                                    .bind(multiscattering_lut)
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
                                    .bind(transmittance_lut, linear_sampler)
                                    .bind(multiscattering_lut, linear_sampler)
                                    .bind(sky_view_lut)
                                    .build();

        graph.add_compute_dispatch(
            ComputeDispatch<glm::vec3>{
                .name = "Compute sky view LUT",
                .descriptor_sets = {set},
                .push_constants = light_vector,
                .num_workgroups = glm::uvec3{(200 / 8) + 1, (200 / 8) + 1, 1},
                .compute_shader = sky_view_lut_pso
            });
    }

    graph.add_transition_pass(
        {
            .textures = {
                {
                    .texture = transmittance_lut,
                    .stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, .access = VK_ACCESS_2_SHADER_READ_BIT,
                    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                },
                {
                    .texture = multiscattering_lut,
                    .stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, .access = VK_ACCESS_2_SHADER_READ_BIT,
                    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                },
                {
                    .texture = sky_view_lut,
                    .stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, .access = VK_ACCESS_2_SHADER_READ_BIT,
                    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                },
            }
        });

    graph.end_label();
}

void ProceduralSky::render_sky(
    CommandBuffer& commands, const BufferHandle view_buffer, const glm::vec3& light_vector,
    const DescriptorSet& gbuffer_descriptor_set
) const {
    auto& backend = RenderBackend::get();

    const auto set = backend.get_transient_descriptor_allocator().build_set(sky_application_pso, 0)
                            .bind(transmittance_lut, linear_sampler)
                            .bind(sky_view_lut, linear_sampler)
                            .bind(view_buffer)
                            .build();

    commands.bind_pipeline(sky_application_pso);

    commands.bind_descriptor_set(0, set);
    commands.bind_descriptor_set(1, gbuffer_descriptor_set);
    commands.set_push_constant(0, light_vector.x);
    commands.set_push_constant(1, light_vector.y);
    commands.set_push_constant(2, light_vector.z);

    commands.draw(3);

    commands.clear_descriptor_set(0);
    commands.clear_descriptor_set(1);
}

TextureHandle ProceduralSky::get_sky_view_lut() const {
    return sky_view_lut;
}

TextureHandle ProceduralSky::get_transmission_lut() const {
    return transmittance_lut;
}
