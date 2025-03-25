#include "irradiance_cache.hpp"

#include <random>

#include "console/cvars.hpp"
#include "core/system_interface.hpp"
#include "glm/gtx/scalar_relational.inl"
#include "render/render_scene.hpp"
#include "render/scene_view.hpp"
#include "render/backend/pipeline_cache.hpp"
#include "render/backend/render_backend.hpp"
#include "shared/gi_probe.hpp"

// Cascade 0 is 16x16x4 meters
// Cascade 1 is 64x64x16 meters
// Cascade 2 is 512x512x128 meters
// Cascade 3 is 8x8x2 kilometers
// I may bring these down if I actually ship a game of some kind

static AutoCVar_Int cvar_rays_per_probe{
    "r.GI.Cache.RaysPerProbe", "How many rays to send out when updating a probe", 400
};
static AutoCVar_Int cvar_probes_per_frame{
    "r.GI.Cache.UpdatesPerFrame", "How many probes we can update per frame", 32
};

static std::shared_ptr<spdlog::logger> logger;

IrradianceCache::Probe& IrradianceCache::ProbeGrid::at(const uint3 index) {
    const auto array_index = index.x + index.y * cascade_size_xz + index.z * cascade_size_xz * cascade_size_y;
    return this->operator[](array_index);
}

void IrradianceCache::Cascade::move_probes() {
    ProbeGrid new_probes;

    for(auto z = 0; z < static_cast<int>(cascade_size_xz); z++) {
        for(auto y = 0; y < static_cast<int>(cascade_size_y); y++) {
            for(auto x = 0; x < static_cast<int>(cascade_size_xz); x++) {
                const auto source_x = x + movement.x;
                const auto source_y = y + movement.y;
                const auto source_z = z + movement.z;

                if(source_x >= 0 && source_x < static_cast<int>(cascade_size_xz) &&
                    source_y >= 0 && source_y < static_cast<int>(cascade_size_y) &&
                    source_z >= 0 && source_z < static_cast<int>(cascade_size_xz)) {
                    new_probes.at({x, y, z}) = probes.at({source_x, source_y, source_z});
                }
            }
        }
    }

    probes = new_probes;
}

IrradianceCache::IrradianceCache() {
    if(logger == nullptr) {
        logger = SystemInterface::get().get_logger("IrradianceCache");
    }

    auto& allocator = RenderBackend::get().get_global_allocator();

    // All these volumes are a little bigger than the number of texels per probe might imply, because we have a one pixel border around each texel

    constexpr auto resolution = glm::uvec3{cascade_size_xz, cascade_size_y * num_cascades, cascade_size_xz};
    constexpr uint3 rtgi_probe_size = {7, 8, 1};
    rtgi_a = allocator.create_volume_texture(
        "probe_rtgi_a",
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        resolution * rtgi_probe_size,
        1,
        TextureUsage::StorageImage);
    rtgi_b = allocator.create_volume_texture(
        "probe_rtgi_b",
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        resolution * rtgi_probe_size,
        1,
        TextureUsage::StorageImage);

    constexpr uint3 light_cache_probe_size = {13, 13, 1};
    light_cache_a = allocator.create_volume_texture(
        "probe_light_cache_a",
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        resolution * light_cache_probe_size,
        1,
        TextureUsage::StorageImage);
    light_cache_b = allocator.create_volume_texture(
        "probe_light_cache_b",
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        resolution * light_cache_probe_size,
        1,
        TextureUsage::StorageImage);

    constexpr uint3 probe_depth_probe_size = {12, 12, 1};
    depth_a = allocator.create_volume_texture(
        "probe_depth_a",
        VK_FORMAT_R16G16_SFLOAT,
        resolution * probe_depth_probe_size,
        1,
        TextureUsage::StorageImage
    );
    depth_b = allocator.create_volume_texture(
        "probe_depth_b",
        VK_FORMAT_R16G16_SFLOAT,
        resolution * probe_depth_probe_size,
        1,
        TextureUsage::StorageImage
    );

    average_a = allocator.create_volume_texture(
        "probe_average_a",
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        resolution,
        1,
        TextureUsage::StorageImage);
    average_b = allocator.create_volume_texture(
        "probe_average_b",
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        resolution,
        1,
        TextureUsage::StorageImage);

    validity_a = allocator.create_volume_texture(
        "probe_validity_a",
        VK_FORMAT_R8_UINT,
        resolution,
        1,
        TextureUsage::StorageImage);
    validity_b = allocator.create_volume_texture(
        "probe_validity_b",
        VK_FORMAT_R8_UINT,
        resolution,
        1,
        TextureUsage::StorageImage);

    probes_to_update_buffer = allocator.create_buffer(
        "probes_to_update",
        sizeof(glm::uvec3) * cvar_probes_per_frame.Get(),
        BufferUsage::StorageBuffer);

    cascade_cbuffer = allocator.create_buffer("cascade_cbuffer", sizeof(ProbeCascade) * 4, BufferUsage::UniformBuffer);

    trace_results_texture = allocator.create_texture(
        "probe_trace_results",
        {
            .format = VK_FORMAT_R16G16B16A16_SFLOAT,
            .resolution = {20, 20},
            .usage = TextureUsage::StorageImage,
            .num_layers = static_cast<uint32_t>(cvar_probes_per_frame.Get()),
        }
    );
    trace_params_texture = allocator.create_texture(
        "probe_trace_params",
        {
            .format = VK_FORMAT_R16G16B16A16_SFLOAT,
            .resolution = {20, 20},
            .usage = TextureUsage::StorageImage,
            .num_layers = static_cast<uint32_t>(cvar_probes_per_frame.Get()),
        }
    );

    linear_sampler = allocator.get_sampler(
        {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
        });
}

IrradianceCache::~IrradianceCache() {
    auto& allocator = RenderBackend::get().get_global_allocator();

    allocator.destroy_texture(rtgi_a);
    allocator.destroy_texture(light_cache_a);
    allocator.destroy_texture(depth_a);
    allocator.destroy_texture(average_a);
    allocator.destroy_texture(validity_a);
    allocator.destroy_texture(rtgi_b);
    allocator.destroy_texture(light_cache_b);
    allocator.destroy_texture(depth_b);
    allocator.destroy_texture(average_b);
    allocator.destroy_texture(validity_b);

    allocator.destroy_buffer(probes_to_update_buffer);
    allocator.destroy_buffer(cascade_cbuffer);

    allocator.destroy_texture(trace_results_texture);
    allocator.destroy_texture(trace_params_texture);
}

void IrradianceCache::update_cascades_and_probes(
    RenderGraph& graph, const SceneView& view, const RenderScene& scene, const TextureHandle noise_tex
) {
    ZoneScoped;

    num_probes_updated = 0;

    place_probes_from_view(view);

    copy_probes_to_new_texture(graph);

    find_probes_to_update(view.get_frame_count());

    dispatch_probe_updates(graph, scene, noise_tex);
}

bool IrradianceCache::request_probe_update(const uint3 probe_index) {
    ZoneScoped;
    if(num_probes_updated == static_cast<uint32_t>(cvar_probes_per_frame.Get())) {
        return false;
    }

    probes_to_update[num_probes_updated] = probe_index;
    num_probes_updated++;

    return true;
}

void IrradianceCache::place_probes_from_view(const SceneView& view) {
    ZoneScoped;

    constexpr auto probe_grid_size = float3{cascade_size_xz, cascade_size_y, cascade_size_xz};

    auto cur_cascade = 0u;
    for(auto& cascade : cascades) {
        const auto cascade_size = cascade.probe_spacing * probe_grid_size;
        const auto forward_pos = view.get_position() + view.get_forward() * cascade_size * 0.5f;
        const auto center_pos = glm::lerp(view.get_position(), forward_pos, cascade.forward_alignment);
        auto min_pos = center_pos - cascade_size * float3{0.5f};

        // TODO: Get this from actual terrain (and also have actual terrain)
        const float min_terrain_height = 0;
        const float volume_terrain_margin = 0.5;
        if(cascade.constrain_to_terrain_max) {
            min_pos.y = glm::min(min_pos.y, min_terrain_height - cascade.probe_spacing * volume_terrain_margin);
        }

        if(cascade.constrain_to_terrain_min) {
            min_pos.y = glm::max(min_pos.y, min_terrain_height - cascade.probe_spacing * volume_terrain_margin);
        }

        // And finally, snap the position to the probe grid
        min_pos = round(min_pos / float3{cascade.probe_spacing}) * cascade.probe_spacing;

        if(min_pos != cascade.location) {
            cascade.movement = (cascade.location - min_pos) / cascade.probe_spacing;
            cascade.location = min_pos;

            if(first_frame) {
                cascade.movement = float3{64};
            } else {
                cascade.move_probes();
            }

            // Find the new probes, add them to the update list
            cascade.probes.foreach(
                [&](const uint3 cur_probe, Probe& probe) {
                    const auto old_probe_location = int3{cur_probe} - int3{cascade.movement};
                    if(glm::any(glm::lessThan(old_probe_location, int3{0})) ||
                        glm::any(greaterThan(old_probe_location, int3{probe_grid_size - 1.f}))) {

                        const auto updated = request_probe_update(
                            uint3{cur_probe.x, cur_probe.y + 8 * cur_cascade, cur_probe.z});
                        if(updated) {
                            probe.is_valid = true;
                            probe.last_update_frame = view.get_frame_count();
                            return true;
                        } else {
                            return false;
                        }
                    }
                    return true;
                });

        } else {
            cascade.movement = float3{0};
        }

        constexpr auto bias_mat = float4x4{
            0.5f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.5f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.5f, 0.0f,
            0.5f, 0.5f, 0.5f, 1.0f
        };

        cascade.world_to_cascade = float4x4{1};
        cascade.world_to_cascade = scale(cascade.world_to_cascade, float3{1.f / cascade.probe_spacing});
        cascade.world_to_cascade = translate(cascade.world_to_cascade, -cascade.location);
        cascade.world_to_cascade = bias_mat * cascade.world_to_cascade;

        cascade.cascade_to_world = glm::inverse(cascade.world_to_cascade);

        cur_cascade++;
    }

    first_frame = false;

    eastl::array<ProbeCascade, 4> cascade_gpu_data;
    for(uint i = 0; i < cascades.size(); i++) {
        cascade_gpu_data[i].min = cascades[i].location;
        cascade_gpu_data[i].probe_spacing = cascades[i].probe_spacing;
    }

    RenderBackend::get().get_upload_queue().upload_to_buffer(cascade_cbuffer, std::span{cascade_gpu_data});
}

void IrradianceCache::copy_probes_to_new_texture(RenderGraph& graph) {
    auto& backend = RenderBackend::get();
    if(cascade_copy_shader == nullptr) {
        cascade_copy_shader = backend.get_pipeline_cache().create_pipeline(
            "shaders/gi/cache/copy_cascades.comp.spv");
    }

    const auto set = backend.get_transient_descriptor_allocator()
                            .build_set(cascade_copy_shader, 0)
                            .bind(rtgi_a)
                            .bind(light_cache_a)
                            .bind(depth_a)
                            .bind(average_a)
                            .bind(validity_a)
                            .bind(rtgi_b)
                            .bind(light_cache_b)
                            .bind(depth_b)
                            .bind(average_b)
                            .bind(validity_b)
                            .build();

    graph.add_compute_dispatch(
        ComputeDispatch<float3[4]>{
            .name = "cascade_copy",
            .descriptor_sets = {set},
            .push_constants = {cascades[0].movement, cascades[1].movement, cascades[2].movement, cascades[3].movement},
            .num_workgroups = {8, 8, 8},
            .compute_shader = cascade_copy_shader
        });

    swap_probe_textures();
}

void IrradianceCache::swap_probe_textures() {
    std::swap(rtgi_a, rtgi_b);
    std::swap(light_cache_a, light_cache_b);
    std::swap(depth_a, depth_b);
    std::swap(average_a, average_b);
    std::swap(validity_a, validity_b);
}

void IrradianceCache::find_probes_to_update(const uint32_t frame_count) {
    ZoneScoped;

    const auto update_budget = static_cast<uint32_t>(cvar_probes_per_frame.Get());
    if(num_probes_updated >= update_budget) {
        return;
    }

    /*
     * Add recently invalidated probes
     *
     * A probe is invalidated if it's near a dynamic object. It's also invalidated if the time of day changes. Well,
     * that's how Ubisoft did it. I don't yet have any dynamic objects, nor do I have dynamic time of day, so we'll
     * skip this part
     */

    auto rng = std::default_random_engine{frame_count};
    auto distribution = std::uniform_real_distribution{0.f, 1.f};

    auto total_weight = 0.f;
    for(const auto& cascade : cascades) {
        total_weight += cascade.update_priority;
    }

    {
        ZoneScopedN("Update invalid probes");
        for(auto& cascade : cascades) {
            const auto normalized_priority = cascade.update_priority / total_weight;
            cascade.probes.foreach(
                [&](const uint3 index, Probe& probe) {
                    if(!probe.is_valid) {
                        float num;
                        {
                            ZoneScopedN("rng1");
                            num = distribution(rng);
                        }
                        if(num < normalized_priority) {
                            const auto updated = request_probe_update(index);
                            if(updated) {
                                probe.is_valid = true;
                                probe.last_update_frame = frame_count;
                            } else {
                                return false;
                            }
                        }
                    }
                    return true;
                });
        }
    }

    if(num_probes_updated >= update_budget) {
        return;
    }

    /*
     * Add probes that haven't been updated in a while
     */
    {
        ZoneScopedN("Update old probes");
        for(auto& cascade : cascades) {
            const auto normalized_priority = cascade.update_priority / total_weight;
            cascade.probes.foreach(
                [&](const uint3 index, Probe& probe) {
                    // Inaccurate, but plausible
                    const auto seconds_since_update = static_cast<float>(frame_count - probe.last_update_frame) / 60.f;
                    const auto update_score = log(seconds_since_update);
                    float num;
                    {
                        ZoneScopedN("rng2");
                        num = distribution(rng);
                    }
                    if(num < update_score * normalized_priority) {
                        const auto updated = request_probe_update(index);
                        if(updated) {
                            probe.is_valid = true;
                            probe.last_update_frame = frame_count;
                        } else {
                            return false;
                        }
                    }
                    return true;
                });
        }
    }

    logger->info("Updating {} probes", num_probes_updated);
}

void IrradianceCache::dispatch_probe_updates(
    RenderGraph& graph, const RenderScene& scene, const TextureHandle noise_tex
) {
    auto& backend = RenderBackend::get();
    backend.get_upload_queue().upload_to_buffer(probes_to_update_buffer, std::span{probes_to_update});

    if(probe_tracing_pipeline == nullptr) {
        probe_tracing_pipeline = backend.get_pipeline_cache()
                                        .create_ray_tracing_pipeline("shaders/gi/cache/probe_tracing.rt.spv");
    }

    // Dispatch rays!
    // We dispatch 400 threads per probe, one dispatch per probe. Each DispatchRays call writes the ray results to a
    // buffer, one buffer per probe. Then, we dispatch a compute shader (one workgroup per probe) to convolve the ray
    // results and write the probe data

    auto& descriptor_allocator = backend.get_transient_descriptor_allocator();

    {
        const auto& sky = scene.get_sky();
        auto set = descriptor_allocator.build_set(probe_tracing_pipeline, 0)
                                       .bind(scene.get_primitive_buffer())
                                       .bind(scene.get_sun_light().get_constant_buffer())
                                       .bind(probes_to_update_buffer)
                                       .bind(scene.get_raytracing_scene().get_acceleration_structure())
                                       .bind(rtgi_a, linear_sampler)
                                       .bind(depth_a, linear_sampler)
                                       .bind(noise_tex)
                                       .bind(validity_a)
                                       .bind(cascade_cbuffer)
                                       .bind(sky.get_transmittance_lut(), sky.get_sampler())
                                       .bind(sky.get_sky_view_lut(), sky.get_sampler())
                                       .bind(trace_params_texture)
                                       .bind(trace_results_texture)
                                       .build();

        graph.add_pass(
            {
                .name = "probe_tracing",
                .descriptor_sets = {set},
                .execute = [&](CommandBuffer& commands) {
                    commands.bind_pipeline(probe_tracing_pipeline);

                    commands.bind_descriptor_set(0, set);
                    commands.bind_descriptor_set(1, backend.get_texture_descriptor_pool().get_descriptor_set());

                    commands.dispatch_rays({20, 20, num_probes_updated});

                    commands.clear_descriptor_set(0);
                }
            });
    }

    {
        if(probe_depth_update_shader == nullptr) {
            probe_depth_update_shader = backend.get_pipeline_cache().create_pipeline(
                "shaders/gi/cache/probe_depth_update.comp.spv");
        }
        auto set = descriptor_allocator.build_set(probe_depth_update_shader, 0)
                                       .bind(probes_to_update_buffer)
                                       .bind(trace_params_texture)
                                       .bind(trace_results_texture)
                                       .bind(depth_a)
                                       .build();

        graph.add_compute_dispatch(
            ComputeDispatch{
                .name = "probe_depth_update",
                .descriptor_sets = {set},
                .num_workgroups = {num_probes_updated, 1, 1},
                .compute_shader = probe_depth_update_shader
            });
    }
    //{
    //    if(probe_light_cache_update_shader == nullptr) {
    //        probe_light_cache_update_shader = backend.get_pipeline_cache().create_pipeline(
    //            "shaders/gi/cache/probe_light_cache_update.comp.spv");
    //    }
    //    auto set = descriptor_allocator.build_set(probe_light_cache_update_shader, 0)
    //        .bind(probes_to_update_buffer)
    //        .bind(trace_params_texture)
    //        .bind(trace_results_texture)
    //        .bind(light_cache_a)
    //        .build();
    //
    //    graph.add_compute_dispatch(
    //        ComputeDispatch{
    //            .name = "probe_light_cache_update",
    //            .descriptor_sets = {set},
    //            .num_workgroups = {num_probes_updated, 1, 1},
    //            .compute_shader = probe_light_cache_update_shader
    //        });
    //}
}
