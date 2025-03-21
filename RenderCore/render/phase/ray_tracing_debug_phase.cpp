#include "ray_tracing_debug_phase.hpp"

#include "console/cvars.hpp"
#include "render/gbuffer.hpp"
#include "render/render_scene.hpp"
#include "render/scene_view.hpp"
#include "render/backend/pipeline_cache.hpp"
#include "render/backend/render_backend.hpp"

enum class RaytracingDebugMode {
    Off,
    BaseColor,
    Normals,
    Data,
    Emission
};

static AutoCVar_Enum cvar_debug_mode{
    "r.RayTracing.DebugMode", "How to debug the scene. 0=off, 1=base color, 2=normals, 3=data, 4=emission",
    RaytracingDebugMode::BaseColor
};

uint32_t RayTracingDebugPhase::get_debug_mode() {
    return static_cast<uint32_t>(cvar_debug_mode.Get());
}

void RayTracingDebugPhase::raytrace(
    RenderGraph& graph, const SceneView& view, const RenderScene& scene, const GBuffer& gbuffer,
    const TextureHandle output_texture
) {
    if(cvar_debug_mode.Get() == RaytracingDebugMode::Off) {
        return;
    }

    auto& backend = RenderBackend::get();

    if(pipeline == nullptr) {
        pipeline = backend.get_pipeline_cache().create_ray_tracing_pipeline("shaders/debug/ray_tracing.rt.raygen.spv");
    }

    auto set = backend.get_transient_descriptor_allocator().build_set(pipeline, 0)
                      .bind(scene.get_primitive_buffer())
                      .bind(view.get_buffer())
                      .bind(gbuffer.depth)
                      .bind(output_texture)
                      .build();

    graph.add_pass(
        {
            .name = "rt_debug",
            .descriptor_sets = {set},
            .execute = [&](CommandBuffer& commands) {
                commands.bind_pipeline(pipeline);

                commands.bind_descriptor_set(0, set);
                commands.bind_descriptor_set(1, backend.get_texture_descriptor_pool().get_descriptor_set());

                commands.dispatch_rays(
                    {output_texture->create_info.extent.width, output_texture->create_info.extent.height});

                commands.clear_descriptor_set(0);
                commands.clear_descriptor_set(1);
            }
        });

}
