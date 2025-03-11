/**
 * Computes ray traced ambient occlusion using spatio-temporal blue noise to guide the ray directions
 */

#include "shared/view_data.hpp"

ConstantBuffer<ViewDataGPU> view_info;

RaytracingAccelerationStructure rtas;

Texture2D<float> depth_buffer;

Texture2D<half3> normal_buffer;

Texture2D<float3> noisetex;

RWTexture2D<float> ao_out;

[vk::push_constant]
cbuffer Constants {
    uint samples_per_pixel;
    float max_ray_distance;
    uint16_t2 output_resolution;
    uint16_t2 noise_tex_xy;
};

float3 get_worldspace_position(uint2 pixel) {
    float depth = depth_buffer[pixel].r;
    float2 texcoord = float2(pixel) / view_info.render_resolution.xy;
    float4 ndc_position = float4(float3(texcoord * 2.f - 1.f, depth), 1.f);
    float4 viewspace_position = mul(view_info.inverse_projection, ndc_position);
    float4 worldspace_position = mul(view_info.inverse_view, viewspace_position);

    return worldspace_position.xyz / worldspace_position.w;
}

[numthreads(8, 8, 1)]
[shader("compute")]
void main(uint2 thread_id: SV_DispatchThreadID) {
    if (any(thread_id >= output_resolution)) {
        return;
    }

    // Get worldspace location from depth buffer, read normal from normalmap
    // Generate a ray in a cosine lobe around the normal
    // Send a ray query
    // If it hits anything opaque, add to a count
    // Normalize the count and write to the output

    const float3 worldspace_position = get_worldspace_position(thread_id);
    const half3 normal = normalize(normal_buffer[thread_id].xyz);

    const uint2 noisetex_pixel = thread_id % noise_tex_xy;

    float ao = samples_per_pixel;

    half3 noise_axis = half3(0, 0, 1);
    half3x3 tbn;
    tbn[0] = normal;
    tbn[2] = normalize(cross(normal, noise_axis));
    tbn[1] = normalize(cross(tbn[2], normal));
    tbn = transpose(tbn);

    for (uint ray_index = 0; ray_index < samples_per_pixel; ray_index++) {
        const float3 noise = normalize(noisetex[noisetex_pixel] * 2.f - 1.f);
        const float3 local_noise = mul(noise, tbn);

        RayDesc ray = (RayDesc)0;
        ray.Origin = worldspace_position;
        ray.Direction = normalize(normal + noise);
        ray.TMin = 0.01;
        ray.TMax = max_ray_distance;

        RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_NON_OPAQUE> query;

        query.TraceRayInline(rtas, 0, 0xFF, ray);

        query.Proceed();

        if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
            ao -= 1;
        }
    }

    ao /= samples_per_pixel;

    ao_out[thread_id] = ao;
}
