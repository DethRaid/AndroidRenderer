#include "rtgi.hpp"

#include "console/cvars.hpp"
#include "render/backend/render_backend.hpp"

static AutoCVar_Int cvar_rays_per_pixel{"r.GI.RT.SamplesPerPixel", "Number of rays to send for each pixel", 1};

static AutoCVar_Int cvar_spatial_ray_reuse{
    "r.GI.RT.SpatialRayReuse",
    "Whether to re-use rays spatially, e.g. use rays from neighboring pixels for the current pixel", 1
};

RayTracedGlobalIllumination::RayTracedGlobalIllumination() {
    auto& backend = RenderBackend::get();
    auto& pipelines = backend.get_pipeline_cache();
    // Create the whole RT pipeline? 
}

RayTracedGlobalIllumination::~RayTracedGlobalIllumination() {
    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();

    allocator.destroy_texture(ray_texture);
    allocator.destroy_texture(ray_irradiance);
}

void RayTracedGlobalIllumination::trace_global_illumination(
    RenderGraph& graph, const SceneView& view, TextureHandle gbuffer_normals, TextureHandle gbuffer_data,
    TextureHandle gbuffer_depth
) {
    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();

    const auto render_resolution = glm::uvec2{
        gbuffer_depth->create_info.extent.width, gbuffer_depth->create_info.extent.height
    };

    if(ray_texture == nullptr) {
        ray_texture = allocator.create_texture(
            "rtgi_params",
            {
                .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                .resolution = render_resolution,
                .usage = TextureUsage::StorageImage
            });
    }
    if(ray_irradiance == nullptr) {
        ray_texture = allocator.create_texture(
            "rtgi_irradiance",
            {
                .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                .resolution = render_resolution,
                .usage = TextureUsage::StorageImage
            });
    }

    graph.add_pass(
        {
            .name = "ray_traced_global_illumination",
            .textures = {},
            .buffers = {},
            .execute = [&](CommandBuffer& commands) {
               // commands.bind_pipeline(rtgi_pipeline);
                commands.dispatch_rays(render_resolution);
            }
        });
}
