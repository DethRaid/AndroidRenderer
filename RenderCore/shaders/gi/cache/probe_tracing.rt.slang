#include "shared/view_data.hpp"
#include "shared/gi_probe.hpp"
#include "rtgi/rtgi_payload.slangi"
#include "common/octahedral.slangi"

[vk::binding(2, 0)]
StructuredBuffer<uint3> probes_to_update;

[vk::binding(3, 0)]
RaytracingAccelerationStructure rtas;

[vk::binding(4, 0)]
Sampler3D<half3> rtgi;

[vk::binding(5, 0)]
Sampler3D<half2> depth;

[vk::binding(6, 0)]
Texture2D<float3> noisetex;

[vk::binding(7, 0)]
Texture3D<half> validity;

[vk::binding(8, 0)]
ConstantBuffer<ProbeCascade[4]> cascades;

[vk::binding(11, 0)]
RWTexture2DArray<half4> trace_params_texture;

[vk::binding(12, 0)]
RWTexture2DArray<half4> trace_results_texture;

float3 get_probe_uv(in uint3 probe_index, in float2 octant_coords, in uint2 num_probe_texels) {
    const float2 total_num_probe_texels = (float2)num_probe_texels + 2.f;
    const float2 texture_size = total_num_probe_texels * float2(32, 8);

    float2 uv = float2(probe_index.xy * total_num_probe_texels) + total_num_probe_texels * 0.5f;
    uv += octant_coords * ((float2)num_probe_texels * 0.5f);
    uv /= texture_size;

    return float3(uv, probe_index.z / 32.f);
}

float3 sample_cascade(in float3 location, in float3 direction, in uint cascade_index) {
    // Convert location to cascade space. Sample the eight probes closest to the location. Sample their depths and 
    // validity to weight the result, then sample their rtgi textures. Divide by weight, return

    const ProbeCascade cascade = cascades[cascade_index];
    const float3 relative_location = location - cascade.min;
    const float3 probespace_location = relative_location / cascade.probe_spacing;
    const float3 min_probe = floor(probespace_location);

    const float3 alpha = clamp((probespace_location - min_probe) / cascade.probe_spacing, 0.f, 1.f);

    const float3 offsets[8] = float3[8](
        float3(0, 0, 0),
        float3(0, 0, 1),
        float3(0, 1, 0),
        float3(0, 1, 1),
        float3(1, 0, 0),
        float3(1, 0, 1),
        float3(1, 1, 0),
        float3(1, 1, 1)
    );

    float3 irradiance = 0.h;
    float weight = 0.h;

    for (uint i = 0; i < 8; i++) {
        const float3 probe_location = min_probe + offsets[i];
        const float3 direction_to_probe = probe_location - probespace_location;
        const float distance_to_probe = length(direction_to_probe) * cascade.probe_spacing;

        const uint3 base_probe_index = (uint3)(probe_location + float3(0, cascade_index * 8.f, 0));

        const half probe_validity = validity[base_probe_index];
        if (probe_validity == 0) {
            continue;
        }

        // Compute trilinear weights based on the distance to each adjacent probe to smoothly transition between
        // probes. offset is binary, so we're using a 1-alpha when offset = 0 and alpha when offset = 1.
        const float3 trilinear = max(0.001f, lerp(1.f - alpha, alpha, offsets[i]));
        const float trilinear_weight = (trilinear.x * trilinear.y * trilinear.z);
        
        float probe_weight = 1.h;

        const float2 depth_octant_coord = get_octahedral_coordinates(-direction_to_probe);
        const float3 depth_uv = get_probe_uv(base_probe_index, depth_octant_coord, uint2(10, 10));
        const half2 depth = depth.SampleLevel(depth_uv, 0);

        const float variance = abs((depth.x * depth.x) - depth.y);

        // Occlusion test
        float chebyshev_weight = 1;
        if (distance_to_probe > depth.x) // occluded
        {
            // v must be greater than 0, which is guaranteed by the if condition above.
            const float v = distance_to_probe - depth.x;
            chebyshev_weight = variance / (variance + (v * v));

            // Increase the contrast in the weight
            chebyshev_weight = max((chebyshev_weight * chebyshev_weight * chebyshev_weight), 0.h);
        }

        // Avoid visibility weights ever going all the way to zero because
        // when *no* probe has visibility we need a fallback value
        probe_weight *= max(0.05, chebyshev_weight);

        // Avoid a weight of zero
        probe_weight = max(0.000001, probe_weight);

        // A small amount of light is visible due to logarithmic perception, so
        // crush tiny weights but keep the curve continuous
        const float crush_threshold = 0.2f;
        if (probe_weight < crush_threshold)
        {
            probe_weight *= (probe_weight * probe_weight) * (1.f / (crush_threshold * crush_threshold));
        }

        probe_weight *= trilinear_weight;

        const float2 irradiance_octant_coord = get_octahedral_coordinates(direction);
        const float3 rtgi_uv = get_probe_uv(base_probe_index, irradiance_octant_coord, uint2(5, 6));
        const half3 probe_irradiance = rtgi.SampleLevel(rtgi_uv, 0);

        irradiance += probe_irradiance * probe_weight;
        weight += probe_weight;
    }

    irradiance /= weight;

    return irradiance * 2 * PI;
}

/**
 * Yeet 400 rays into the aether. Convolve them into our textures. Party
 *
 * This shader doesn't update the irradiance cache itself. Rather, we write the new probe data to the new_probe_data
 * buffer. A later compute shader will take that information, generate the RTGI and Average data, and write it all to 
 * irradiance textures. This helps with sync - the RT shaders can sample the irradiance textures as needed, without
 * race conditions between this shader
 */
[shader("raygen")]
void main_raygen() {
    const uint3 thread_id = DispatchRaysIndex();
    const uint probe_index = thread_id.z;

    const uint3 probe_id = probes_to_update[probe_index];

    const uint cascade_index = probe_id.y / 8;
    const ProbeCascade cascade = cascades[cascade_index];

    const float3 local_probe_id = (float3)uint3(probe_id.x, probe_id.y % 8, probe_id.z);

    const float2 octant_ray_coord = get_normalized_octahedral_coordinates(thread_id.xy, 20);
    const float3 ray_direction = get_octahedral_direction(octant_ray_coord);

    float ray_distance = cascade.probe_spacing * 4;
    if (cascade_index < 3) {
        ray_distance = cascades[cascade_index + 1].probe_spacing * 8;
    }

    RayDesc ray = (RayDesc)0;
    ray.Origin = cascade.min + local_probe_id * cascade.probe_spacing;
    ray.Direction = ray_direction;
    ray.TMin = 0;
    ray.TMax = ray_distance;

    IrradiancePayload payload = (IrradiancePayload)0;
    payload.remaining_bounces = 0;

    TraceRay(
        rtas,
        RAY_FLAG_NONE,
        0xFF,
        RAY_TYPE_GI,
        1,
        0,
        ray,
        payload);

    // TODO: If the ray misses (trace distance = 0), sample the next-highest cascade (or the sky, if we're already in cascade 3)

    if (payload.ray_distance == 0) {
        // If we didn't hit anything and there's a next-largest cascade to sample, sample from it. Otherwise we just 
        // keep the sky irradiance in the payload
        if(cascade_index + 1 < 4) {
            payload.irradiance = sample_cascade(ray.Origin + ray.Direction * ray.TMax, ray.Direction, cascade_index + 1);
        }

        payload.ray_distance = ray_distance;
    } else if (payload.ray_distance < 0) {
        // We hit the back of a surface. Set this ray to black so we don't mess up our results
        payload.irradiance = 0;
    }

    trace_params_texture[thread_id] = half4((half3)ray_direction, 0);
    trace_results_texture[thread_id] = half4((half3)payload.irradiance, (half)payload.ray_distance);
}
