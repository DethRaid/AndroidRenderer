#include "irradiance_cache.hpp"

#include "console/cvars.hpp"
#include "render/scene_view.hpp"
#include "render/backend/pipeline_cache.hpp"
#include "render/backend/render_backend.hpp"

// Cascade 0 is 16x16x4 meters
// Cascade 1 is 64x64x16 meters
// Cascade 2 is 512x512x128 meters
// Cascade 3 is 8x8x2 kilometers
// I may bring these down if I actually ship a game of some kind

static AutoCVar_Int cvar_rays_per_probe{
    "r.GI.Cache.RaysPerProbe", "How many rays to send out when updating a probe", 400
};
static AutoCVar_Int cvar_probes_per_frame{
    "r.GI.Cache.UpdatesPerFrame", "How many probes we can update per frame", 1024
};

IrradianceCache::IrradianceCache() {
    auto& allocator = RenderBackend::get().get_global_allocator();

    constexpr auto resolution = glm::uvec3{cascade_size_xz, cascade_size_y * num_cascades, cascade_size_xz};
    rtgi_a = allocator.create_volume_texture(
        "probe_rtgi_a",
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        resolution * glm::uvec3{8, 8, 1},
        1,
        TextureUsage::StorageImage);
    rtgi_b = allocator.create_volume_texture(
        "probe_rtgi_b",
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        resolution * glm::uvec3{8, 8, 1},
        1,
        TextureUsage::StorageImage);

    light_cache_a = allocator.create_volume_texture(
        "probe_light_cache_a",
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        resolution * glm::uvec3{11, 11, 1},
        1,
        TextureUsage::StorageImage);
    light_cache_b = allocator.create_volume_texture(
        "probe_light_cache_b",
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        resolution * glm::uvec3{11, 11, 1},
        1,
        TextureUsage::StorageImage);

    depth_a = allocator.create_volume_texture(
        "probe_depth_a",
        VK_FORMAT_R16G16_SFLOAT,
        resolution * glm::uvec3{10, 10, 1},
        1,
        TextureUsage::StorageImage
    );
    depth_b = allocator.create_volume_texture(
        "probe_depth_b",
        VK_FORMAT_R16G16_SFLOAT,
        resolution * glm::uvec3{10, 10, 1},
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
        VK_FORMAT_R8_UNORM,
        resolution,
        1,
        TextureUsage::StorageImage);
    validity_b = allocator.create_volume_texture(
        "probe_validity_b",
        VK_FORMAT_R8_UNORM,
        resolution,
        1,
        TextureUsage::StorageImage);
}

IrradianceCache::IrradianceCache(IrradianceCache&& old) noexcept :
    rtgi_a{old.rtgi_a},
    light_cache_a{old.light_cache_a},
    depth_a{old.depth_a},
    average_a{old.average_a},
    validity_a{old.validity_a},
    rtgi_b{old.rtgi_b},
    light_cache_b{old.light_cache_b},
    depth_b{old.depth_b},
    average_b{old.average_b},
    validity_b{old.validity_b} {

    old.rtgi_a = nullptr;
    old.light_cache_a = nullptr;
    old.depth_a = nullptr;
    old.average_a = nullptr;
    old.validity_a = nullptr;
    old.rtgi_b = nullptr;
    old.light_cache_b = nullptr;
    old.depth_b = nullptr;
    old.average_b = nullptr;
    old.validity_b = nullptr;
}

IrradianceCache& IrradianceCache::operator=(IrradianceCache&& old) noexcept {
    this->~IrradianceCache();

    rtgi_a = old.rtgi_a;
    light_cache_a = old.light_cache_a;
    depth_a = old.depth_a;
    average_a = old.average_a;
    validity_a = old.validity_a;
    rtgi_b = old.rtgi_b;
    light_cache_b = old.light_cache_b;
    depth_b = old.depth_b;
    average_b = old.average_b;
    validity_b = old.validity_b;

    old.rtgi_a = nullptr;
    old.light_cache_a = nullptr;
    old.depth_a = nullptr;
    old.average_a = nullptr;
    old.validity_a = nullptr;
    old.rtgi_b = nullptr;
    old.light_cache_b = nullptr;
    old.depth_b = nullptr;
    old.average_b = nullptr;
    old.validity_b = nullptr;

    return *this;
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
}

void IrradianceCache::place_probes_from_view(const SceneView& view) {
    constexpr auto probe_grid_size = float3{cascade_size_xz, cascade_size_y, cascade_size_xz};

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

        if(first_frame) {
            cascade.movement = float3{0};
        }
    }

    first_frame = false;
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

    std::swap(rtgi_a, rtgi_b);
    std::swap(light_cache_a, light_cache_b);
    std::swap(depth_a, depth_b);
    std::swap(average_a, average_b);
    std::swap(validity_a, validity_b);
}
