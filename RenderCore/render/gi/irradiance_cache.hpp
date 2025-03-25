#pragma once

#include <array>
#include <vector>
#include <tracy/Tracy.hpp>
#include <vulkan/vulkan_core.h>

#include "shared/prelude.h"
#include "render/backend/handles.hpp"

class RenderScene;
class RenderGraph;
class SceneView;

/**
 * Irradiance cache, based on DDGI
 *
 * Incorporates ideas from Ubisoft's Snowdrop engine
 * https://gdcvault.com/play/1034763/Advanced-Graphics-Summit-Raytracing-in and lpotrick's Timberdoodle engine
 * https://github.com/Sunset-Flock/Timberdoodle
 *
 * Some key differences from Ubisoft's approach:
 * - My probes are much closer together - 0.5m between probes in the smallest cascade. This is because I don't have the
 *      desire nor the budget to make a massive open world
 * - My RT scene contains full-LOD meshes, and I sample material textures in my hit shaders. I'm aiming for relatively
 *      low-poly content, somewhere around a PS3 game, while Ubisoft is making ultra high-res meshes. Additionally, I'm
 *      using a basic PBR material with no real shader logic, while Ubisoft has complicated shader graphs
 * - Ubisoft's presentation mentions that traces return a gbuffer-like structure, and they shade separately. That
 *      reduces the runtime by about 2% (page 60). That seems like a lot of complexity for not much gain
 * - I'm not doing screen-space traces with hardware RT as a fallback, but I'd like to
 * - Ubisoft has shadowmaps for their sun, and perhaps other lights. They use those when shading. I don't - it's all
 *      raytraced. I think I'll send one ray towards the sun, and select a few local lights with ReSTIR
 */
class IrradianceCache {
public:
    static constexpr uint32_t cascade_size_xz = 32;
    static constexpr uint32_t cascade_size_y = 8;

    static constexpr uint32_t num_cascades = 4;

    /**
     * Some CPU-side information about a probe
     */
    struct Probe {
        /**
         * Marks if the probe has been invalidated for any reason
         */
        bool is_valid = false;

        /**
         * The frame where this probe was most recently updated
         */
        uint last_update_frame = 0;
    };

    struct ProbeGrid : std::array<Probe, cascade_size_xz * cascade_size_y * cascade_size_xz> {
        template <typename F>
        void foreach(F func);

        Probe& at(uint3 index);
    };

    struct Cascade {
        /**
         * Distance between probes, in meters
         */
        float probe_spacing = 0;

        /**
         * How much of the cascade is in front of the camera. 0 = centered on camera, 1 = barely includes camera
         */
        float forward_alignment = 0;

        /**
         * Whether we should constrain the cascade to the minimum terrain height. Prevents too much of the cascade
         * going under the terrain and wasting probes
         */
        bool constrain_to_terrain_min = true;

        /**
         * Whether we should constrain the cascade to the maximum terrain height. Prevents too much of the cascade
         * from being up in the air where no one will sample it
         */
        bool constrain_to_terrain_max = false;

        /**
         * How important are this cascade's probes
         */
        float update_priority = 0.1f;

        /**
         * Worldspace location of the min of the bounds of this cascade
         */
        float3 location = {};

        /**
         * How far the cascade has moved, measured in probes. Used when copying old probes to the new volume
         */
        int3 movement = {};

        /**
         * Transforms from worldspace to pixel in the probe texture
         */
        float4x4 world_to_cascade = float4x4{1};

        /**
         * Transforms from pixel in the probe texture to worldspace
         */
        float4x4 cascade_to_world = float4x4{1};

        ProbeGrid probes;

        void move_probes();
    };

    IrradianceCache();

    IrradianceCache(const IrradianceCache& other) = delete;
    IrradianceCache& operator=(const IrradianceCache& other) = delete;

    IrradianceCache(IrradianceCache&& old) = delete;
    IrradianceCache& operator=(IrradianceCache&& old) = delete;

    ~IrradianceCache();

    void update_cascades_and_probes(
        RenderGraph& graph, const SceneView& view, const RenderScene& scene, TextureHandle noise_tex
    );

private:
    /**
     * We can skip copying the cascade textures if this is the first frame
     */
    bool first_frame = true;

    /**
     * Stores 8x8 R11G11B10 textures representing the incoming light at each probe
     */
    TextureHandle rtgi_a = nullptr;
    TextureHandle rtgi_b = nullptr;

    /**
     * Stores 11x11 R11G11B10 textures used as a fallback when tracing rays. Essentially a less-averaged version of rtgi
     */
    TextureHandle light_cache_a = nullptr;
    TextureHandle light_cache_b = nullptr;

    /**
     * 10x10 R8 textures storing the depth around each probe, and also depth squared
     */
    TextureHandle depth_a = nullptr;
    TextureHandle depth_b = nullptr;

    /**
     * Average of the irradiance at this probe. Used for volumetrics, which I will totally code up at some point
     */
    TextureHandle average_a = nullptr;
    TextureHandle average_b = nullptr;

    /**
     * Single int saying if this probe is valid or not. Invalid probes lie entirely inside of an object. They have no
     * impact on the final scene
     */
    TextureHandle validity_a = nullptr;
    TextureHandle validity_b = nullptr;

    std::array<Cascade, num_cascades> cascades = {
        Cascade{
            .probe_spacing = 0.5f,
            .forward_alignment = 0.4f,
            .update_priority = 0.1f,
        },
        {
            .probe_spacing = 2.f,
            .forward_alignment = 0.5f,
            .update_priority = 0.02f,
        },
        {
            .probe_spacing = 16.f,
            .forward_alignment = 0.2f,
            .update_priority = 0.02f,
        },
        {
            .probe_spacing = 256.f,
            .forward_alignment = 0.f,
            .update_priority = 0.02f,
        },
    };

    BufferHandle cascade_cbuffer = nullptr;

    uint32_t num_probes_updated = 0;
    std::array<glm::uvec3, 1024> probes_to_update = {};
    BufferHandle probes_to_update_buffer = nullptr;

    /**
     * Array texture for storing trace results. 20x20 resolution, 1024 layers
     *
     * This texture stores the irradiance (rgb) and ray distance (a)
     */
    TextureHandle trace_results_texture = nullptr;

    /**
     * Array texture for storing tracing parameters. 20x20 resolution, 1024 layers
     *
     * This texture stores the ray direction (rgb)
     */
    TextureHandle trace_params_texture = nullptr;

    VkSampler linear_sampler;

    static inline ComputePipelineHandle cascade_copy_shader = nullptr;

    static inline RayTracingPipelineHandle probe_tracing_pipeline = nullptr;

    static inline ComputePipelineHandle probe_depth_update_shader = nullptr;

    static inline ComputePipelineHandle probe_light_cache_update_shader = nullptr;

    static inline ComputePipelineHandle probe_rtgi_update_shader = nullptr;

    static inline ComputePipelineHandle probe_validity_update_shader = nullptr;

    /**
     * Requests an update of a given probe. Returns true if the probe can be updated this frame, false otherwise
     *
     * The probe index Y MUST contain an offset for the cascade it's in. Otherwise we don't know which cascade we're in :(
     */
    bool request_probe_update(uint3 probe_index);

    /**
     * Updates cascade locations based on the provided view
     */
    void place_probes_from_view(const SceneView& view);

    /**
     * Copies probes from the A texture to the B texture, so that they're in the right location. Out-of-bounds probes
     * are lost
     */
    void copy_probes_to_new_texture(RenderGraph& graph);

    void swap_probe_textures();

    /**
     * Determines which probes should be updated, using a heuristic based on time since update and distance from the
     * center of the screen
     */
    void find_probes_to_update(uint32_t frame_count);

    void dispatch_probe_updates(RenderGraph& graph, const RenderScene& scene, TextureHandle noise_tex);
};

template <typename F>
void IrradianceCache::ProbeGrid::foreach(F func) {
    ZoneScoped;
    for(auto x = 0u; x < cascade_size_xz; x++) {
        for(auto y = 0u; y < cascade_size_y; y++) {
            for(auto z = 0u; z < cascade_size_xz; z++) {
                const auto keep_looping = func(uint3{x, y, z}, at({x, y, z}));
                if(!keep_looping) {
                    return;
                }
            }
        }
    }
}
