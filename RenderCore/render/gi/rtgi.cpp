#include "rtgi.hpp"

#include "console/cvars.hpp"
#include "render/gbuffer.hpp"
#include "render/render_scene.hpp"
#include "render/scene_view.hpp"
#include "render/backend/pipeline_cache.hpp"
#include "render/backend/render_backend.hpp"

static AutoCVar_Int cvar_num_bounces{"r.GI.NumBounces", "Number of times light can bounce in GI. 0 = no GI", 1};

static AutoCVar_Int cvar_num_reconstruction_rays{
    "r.GI.Reconstruction.NumSamples",
    "Number of extra rays to use in the screen-space reconstruction filter, DLSS likes 8, FSR likes 32", 0
};

static AutoCVar_Float cvar_reconstruction_size{
    "r.GI.Reconstruction.Size", "Size in pixels of the screenspace reconstruction filter", 16
};

static AutoCVar_Int cvar_gi_cache{"r.GI.Cache.Enabled", "Whether to enable the GI irradiance cache", true};

static AutoCVar_Int cvar_gi_cache_debug{"r.GI.Cache.Debug", "Enable a debug draw of the irradiance cache", true};

bool RayTracedGlobalIllumination::should_render() {
    return cvar_num_bounces.Get() > 0;
}

RayTracedGlobalIllumination::RayTracedGlobalIllumination() {
    auto& backend = RenderBackend::get();
    if(overlay_pso == nullptr) {
        overlay_pso = backend.begin_building_pipeline("rtgi_application")
                             .set_vertex_shader("shaders/common/fullscreen.vert.spv")
                             .set_fragment_shader("shaders/rtgi/overlay.frag.spv")
                             .set_depth_state(
                                 {
                                     .enable_depth_write = false,
                                     .compare_op = VK_COMPARE_OP_LESS
                                 })
                             .set_blend_state(
                                 0,
                                 {
                                     .blendEnable = VK_TRUE,
                                     .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                     .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                     .colorBlendOp = VK_BLEND_OP_ADD,
                                     .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                     .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                     .alphaBlendOp = VK_BLEND_OP_ADD,
                                     .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                                 }
                             )
                             .build();
    }
}

RayTracedGlobalIllumination::~RayTracedGlobalIllumination() {
    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();

    allocator.destroy_texture(ray_texture);
    allocator.destroy_texture(ray_irradiance);
}

void RayTracedGlobalIllumination::pre_render(
    RenderGraph& graph, const SceneView& view, const RenderScene& scene, const TextureHandle noise_tex
) {
    ZoneScoped;

    if(cvar_gi_cache.Get() == 0) {
        irradiance_cache = nullptr;
    } else if(irradiance_cache == nullptr) {
        irradiance_cache = std::make_unique<IrradianceCache>();
    }

    if(irradiance_cache) {
        irradiance_cache->update_cascades_and_probes(graph, view, scene, noise_tex);
    }
}

void RayTracedGlobalIllumination::post_render(
    RenderGraph& graph, const SceneView& view, const RenderScene& scene, const GBuffer& gbuffer,
    const TextureHandle noise_tex
) {
    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();

    const auto render_resolution = gbuffer.depth->get_resolution();

    if(ray_texture == nullptr || ray_texture->get_resolution() != render_resolution) {
        allocator.destroy_texture(ray_texture);
        ray_texture = allocator.create_texture(
            "rtgi_params",
            {
                .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                .resolution = render_resolution,
                .usage = TextureUsage::StorageImage
            });
    }
    if(ray_irradiance == nullptr || ray_irradiance->get_resolution() != render_resolution) {
        allocator.destroy_texture(ray_irradiance);
        ray_irradiance = allocator.create_texture(
            "rtgi_irradiance",
            {
                .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                .resolution = render_resolution,
                .usage = TextureUsage::StorageImage
            });
    }
    if(rtgi_pipeline == nullptr) {
        rtgi_pipeline = backend.get_pipeline_cache()
                               .create_ray_tracing_pipeline("shaders/rtgi/rtgi.rt.spv");
    }

    const auto sun_buffer = scene.get_sun_light().get_constant_buffer();

    auto& sky = scene.get_sky();
    const auto set = backend.get_transient_descriptor_allocator()
                            .build_set(rtgi_pipeline, 0)
                            .bind(scene.get_primitive_buffer())
                            .bind(sun_buffer)
                            .bind(view.get_buffer())
                            .bind(scene.get_raytracing_scene().get_acceleration_structure())
                            .bind(gbuffer.normals)
                            .bind(gbuffer.depth)
                            .bind(noise_tex)
                            .bind(ray_texture)
                            .bind(ray_irradiance)
                            .bind(sky.get_sky_view_lut(), sky.get_sampler())
                            .bind(sky.get_transmittance_lut(), sky.get_sampler())
                            .build();

    graph.add_pass(
        {
            .name = "ray_traced_global_illumination",
            .descriptor_sets = {set},
            .execute = [&](CommandBuffer& commands) {
                commands.bind_pipeline(rtgi_pipeline);

                commands.bind_descriptor_set(0, set);
                commands.bind_descriptor_set(1, backend.get_texture_descriptor_pool().get_descriptor_set());

                commands.set_push_constant(0, static_cast<uint32_t>(cvar_num_bounces.Get()));

                commands.dispatch_rays(render_resolution);

                commands.clear_descriptor_set(0);
                commands.clear_descriptor_set(1);
            }
        });
}

void RayTracedGlobalIllumination::get_lighting_resource_usages(
    TextureUsageList& textures, BufferUsageList& buffers
) const {
    textures.emplace_back(
        ray_texture,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    textures.emplace_back(
        ray_irradiance,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    if(irradiance_cache) {
        irradiance_cache->get_resource_uses(textures, buffers);
    }
}

void RayTracedGlobalIllumination::render_to_lit_scene(
    CommandBuffer& commands, const BufferHandle view_buffer, TextureHandle ao_tex, const TextureHandle noise_texture
) const {

    if(cvar_gi_cache_debug.Get() != 0 && irradiance_cache != nullptr) {
        irradiance_cache->add_to_lit_scene(commands, view_buffer);
    } else {
        auto set = RenderBackend::get().get_transient_descriptor_allocator()
                                       .build_set(overlay_pso, 1)
                                       .bind(view_buffer)
                                       .bind(noise_texture)
                                       .bind(ray_texture)
                                       .bind(ray_irradiance)
                                       .build();

        commands.set_cull_mode(VK_CULL_MODE_NONE);

        commands.bind_pipeline(overlay_pso);

        commands.bind_descriptor_set(1, set);

        commands.set_push_constant(0, static_cast<uint32_t>(cvar_num_reconstruction_rays.Get()));
        commands.set_push_constant(1, static_cast<float>(cvar_reconstruction_size.Get()));

        commands.draw_triangle();

        commands.clear_descriptor_set(1);
    }
}

void RayTracedGlobalIllumination::draw_debug_overlays(
    RenderGraph& graph, const SceneView& view, const GBuffer& gbuffer, TextureHandle lit_scene_texture
) {
    if(cvar_gi_cache_debug.Get() != 0 && irradiance_cache) {
        irradiance_cache->draw_debug_overlays(graph, view, gbuffer, lit_scene_texture);
    }
}
