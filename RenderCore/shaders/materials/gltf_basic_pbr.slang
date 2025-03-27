#include "common/brdf.slangi"
#include "shared/basic_pbr_material.hpp"
#include "shared/lpv.hpp"
#include "shared/primitive_data.hpp"
#include "shared/sun_light_constants.hpp"
#include "shared/vertex_data.hpp"
#include "shared/view_data.hpp"

#ifndef SAH_DEPTH_ONLY
#define SAH_DEPTH_ONLY 0
#endif

#ifndef SAH_MULTIVIEW
#define SAH_MULTIVIEW 0
#endif

#ifndef SAH_MASKED
#define SAH_MASKED 0
#endif

#ifndef SAH_THIN_GBUFFER
#define SAH_THIN_GBUFFER 0
#endif

#ifndef SAH_GBUFFERS
#define SAH_GBUFFERS 0
#endif

#ifndef SAH_MAIN_VIEW
#define SAH_MAIN_VIEW 0
#endif

#ifndef SAH_CSM
#define SAH_CSM 0
#endif

#ifndef SAH_RSM
#define SAH_RSM 0
#endif

#ifndef SAH_RT
#define SAH_RT 0
#endif

#ifndef SAH_RT_OCCLUSION
#define SAH_RT_OCCLUSION 0
#endif

#ifndef SAH_RT_GI
#define SAH_RT_GI 0
#endif

[vk::binding(0, 0)]
StructuredBuffer<PrimitiveDataGPU> primitive_datas;

#if SAH_MAIN_VIEW
ConstantBuffer<ViewDataGPU> view_data;
#endif

#if SAH_MULTIVIEW
ConstantBuffer<float4x4[4]> world_to_ndc_matrices;
#endif

#if SAH_CSM
[vk::push_constant]
cbuffer Constants {
    uint primitive_id_in;
}
#endif

#if SAH_RSM
ConstantBuffer<SunLightConstants> sun_light;

[vk::push_constant]
cbuffer Constants {
    uint primitive_id_in;
}
#endif

[vk::binding(0, 1)]
Sampler2D textures[];

struct VertexOutput {
    float4 position : SV_Position;
    float2 texcoord;
    half4 color;
    uint primitive_id;
#if SAH_MAIN_VIEW
    float3 viewspace_position;
#endif
#if !SAH_DEPTH_ONLY
    half3 normal;
    half4 tangent;
#endif
};

struct GbufferOutput {
    half4 base_color;
    half4 normal;
    half4 data;
    half4 emission;
};

struct RsmOutput {
    half4 flux;
    half4 normal;
};

#if !SAH_RT
[shader("vertex")]
VertexOutput main_vs(
    const float3 position_in,
    const float3 normal_in,
    const float4 tangent_in,
    const float2 texcoord_in,
    const half4 color_in,
#if SAH_MAIN_VIEW
    const uint primitive_id_in
#endif
#if SAH_MULTIVIEW
    const uint view_id : SV_ViewId
#endif
) {
    VertexOutput output;

    const uint primitive_id = primitive_id_in;
    output.primitive_id = primitive_id;

    PrimitiveDataGPU data = primitive_datas[primitive_id];

#if SAH_MAIN_VIEW
    float4 viewspace_position = mul(view_data.view, mul(data.model, float4(position_in, 1.f)));
    output.position = mul(view_data.projection, viewspace_position);
    output.viewspace_position = viewspace_position.xyz;
#elif SAH_MULTIVIEW
    output.position = mul(world_to_ndc_matrices[view_id], mul(data.model, float4(position_in, 1.f)));
#endif

    output.texcoord = texcoord_in;
    output.color = color_in;

#if !SAH_DEPTH_ONLY
    output.normal = (half3)normalize(mul((float3x3)data.model, normal_in));
    output.tangent.xyz = (half3)normalize(mul((float3x3)data.model, tangent_in.xyz));
    output.tangent.w = (half)tangent_in.w;
#endif

    return output;
}

float3x3 cotangent_frame(float3 N, float3 p, float2 uv) {
    // get edge vectors of the pixel triangle
    float3 dp1 = ddx(p);
    float3 dp2 = ddy(p);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    // solve the linear system
    float3 dp2perp = cross(dp2, N);
    float3 dp1perp = cross(N, dp1);
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    // construct a scale-invariant frame
    float invmax = rsqrt(max(dot(T, T), dot(B, B)));
    return float3x3(T * invmax, B * invmax, N);
}

[shader("fragment")]
#if SAH_RSM
RsmOutput
#else
GbufferOutput
#endif
main_fs(const VertexOutput vertex) {

    PrimitiveDataGPU primitive = primitive_datas[vertex.primitive_id];
    BasicPbrMaterialGpu material = *primitive.material;

    GbufferOutput gbuffer = (GbufferOutput)0;

    float mip_bias = 0;
#if SAH_MAIN_VIEW
    mip_bias = view_data.material_texture_mip_bias;
#endif

    // Base color
    Sampler2D base_color_texture = textures[material.base_color_texture_index];
    half4 base_color_sample = (half4)base_color_texture.SampleBias(vertex.texcoord, mip_bias);
    half4 tinted_base_color = base_color_sample * vertex.color * (half4)material.base_color_tint;

#if SAH_MASKED
    if (tinted_base_color.a <= material.opacity_threshold) {
        discard;
    }
#endif  // SAH_MASKED

#if !SAH_DEPTH_ONLY
    gbuffer.base_color = tinted_base_color;

    // Normals
#if SAH_MAIN_VIEW
    // float3x3 tbn = cotangent_frame(vertex.normal, vertex.viewspace_position, vertex.texcoord);
    half3 bitangent = cross(vertex.normal, vertex.tangent.xyz) * vertex.tangent.w;
    half3x3 tbn = half3x3(
        vertex.tangent.xyz,
        bitangent,
        vertex.normal
    );
    Sampler2D normals_texture = textures[material.normal_texture_index];
    half3 normal_sample = (half3)normals_texture.SampleBias(vertex.texcoord, mip_bias).xyz * 2.h - 1.h;
    half3 normal = mul(normal_sample, tbn);
    gbuffer.normal = half4(normal, 0);
#else
    gbuffer.normal = half4(vertex.normal, 0);
#endif

#if !SAH_RSM
    // Data
    Sampler2D data_texture = textures[material.data_texture_index];
    half4 data_sample = (half4)data_texture.SampleBias(vertex.texcoord, mip_bias);
    half4 tinted_data = data_sample * half4(0, (half)material.roughness_factor, (half)material.metalness_factor, 0);

    gbuffer.data = tinted_data;

    // Emission
    Sampler2D emission_texture = textures[material.emission_texture_index];
    half4 emission_sample = (half4)emission_texture.SampleBias(vertex.texcoord, mip_bias);
    half4 tinted_emission = emission_sample * (half4)material.emission_factor;

    gbuffer.emission = tinted_emission;
#endif  // !SAH_THIN_GBUFFER

#endif  // !SAH_DEPTH_ONLY

#if !SAH_RSM
    return gbuffer;

#else
    SurfaceInfo surface = (SurfaceInfo)0;
    surface.base_color = gbuffer.base_color.rgb;
    surface.normal = gbuffer.normal.xyz;
    surface.metalness = gbuffer.data.b;
    surface.roughness = gbuffer.data.g;

    const half3 brdf_result = Fd(surface, (half3)-sun_light.direction_and_tan_size.xyz, surface.normal);

    RsmOutput output;
    output.flux = half4(brdf_result, 1);
    output.normal = half4(gbuffer.normal.xyz * 0.5h + 0.5h, 1);
    return output;
#endif
}

#else // SAH_RT

half4 unpackUnorm4x8ToHalf(in uint packed) {
    half4 value;
    value.x = (half)(packed & 0xff) / 255.h;
    value.y = (half)((packed >> 8) & 0xff) / 255.h;
    value.z = (half)((packed >> 16) & 0xff) / 255.h;
    value.w = (half)((packed >> 24) & 0xff) / 255.h;

    return value;
}

uint packUnorm4x8(in half4 value) {
    uint packed = 0;
    packed |= (uint)(value.x * 255.h);
    packed |= (uint)(value.y * 255.h) << 8;
    packed |= (uint)(value.z * 255.h) << 16;
    packed |= (uint)(value.w * 255.h) << 24;

    return packed;
}

StandardVertexData interpolate_vertex(const StandardVertexData v0, const StandardVertexData v1, const StandardVertexData v2, const float2 bary)
{
    float3 barycentrics = float3(1.f - bary.x - bary.y, bary.x, bary.y);
    StandardVertexData v = (StandardVertexData)0;
    v.normal = barycentrics.x * v0.normal + barycentrics.y * v1.normal + barycentrics.z * v2.normal;
    v.tangent = barycentrics.x * v0.tangent + barycentrics.y * v1.tangent + barycentrics.z * v2.tangent;
    v.texcoord = barycentrics.x * v0.texcoord + barycentrics.y * v1.texcoord + barycentrics.z * v2.texcoord;
    v.color = packUnorm4x8(barycentrics.x * unpackUnorm4x8ToHalf(v0.color) + barycentrics.y * unpackUnorm4x8ToHalf(v1.color) + barycentrics.z * unpackUnorm4x8ToHalf(v2.color));

    return v;
}

#if SAH_RT_OCCLUSION
#if SAH_MASKED
[shader("anyhit")]
void main_anyhit(inout uint payload, in float2 barycentrics) {
    const uint primitive_id = InstanceID();
    const uint triangle_id = PrimitiveIndex();

    const PrimitiveDataGPU primitive = primitive_datas[primitive_id];

    const uint index = triangle_id * 3;

    const uint i0 = primitive.indices[index];
    const uint i1 = primitive.indices[index + 1];
    const uint i2 = primitive.indices[index + 2];

    const StandardVertexData v0 = primitive.vertex_data[i0];
    const StandardVertexData v1 = primitive.vertex_data[i1];
    const StandardVertexData v2 = primitive.vertex_data[i2];

    const StandardVertexData v = interpolate_vertex(v0, v1, v2, barycentrics);

    const BasicPbrMaterialGpu material = *primitive.material;

    Sampler2D base_color_texture = textures[material.base_color_texture_index];
    const float4 base_color_sample = base_color_texture.SampleLevel(v.texcoord, 0);
    const float4 base_color = base_color_sample * material.base_color_tint * unpackUnorm4x8ToHalf(v.color);

    if (base_color.a <= material.opacity_threshold) {
        IgnoreHit();
    }
}
#endif // SAH_MASKED

[shader("closesthit")]
void main_closesthit(inout uint payload) {
    payload = 0;
}
#endif // SAH_RT_OCCLUSION

#if SAH_RT_GI

#if SAH_MASKED
[shader("anyhit")]
void main_anyhit(inout IrradiancePayload payload, in float2 barycentrics) {
    const uint primitive_id = InstanceID();
    const uint triangle_id = PrimitiveIndex();

    const PrimitiveDataGPU primitive = primitive_datas[primitive_id];

    const uint index = triangle_id * 3;

    const uint i0 = primitive.indices[index];
    const uint i1 = primitive.indices[index + 1];
    const uint i2 = primitive.indices[index + 2];

    const StandardVertexData v0 = primitive.vertex_data[i0];
    const StandardVertexData v1 = primitive.vertex_data[i1];
    const StandardVertexData v2 = primitive.vertex_data[i2];

    const StandardVertexData v = interpolate_vertex(v0, v1, v2, barycentrics);

    const BasicPbrMaterialGpu material = *primitive.material;

    Sampler2D base_color_texture = textures[material.base_color_texture_index];
    const float4 base_color_sample = base_color_texture.SampleLevel(v.texcoord, 0);
    const float4 base_color = base_color_sample * material.base_color_tint * unpackUnorm4x8ToHalf(v.color);

    if (base_color.a <= material.opacity_threshold) {
        IgnoreHit();
    }
}
#endif // SAH_MASKED

#include "rtgi/rtgi_payload.slangi"

[vk::binding(1, 0)]
ConstantBuffer<SunLightConstants> sun_light;

[vk::binding(3, 0)]
RaytracingAccelerationStructure rtas;

[vk::binding(6, 0)]
Texture2D<float3> noisetex;

#ifndef RAY_TYPE_OCCLUSION
#define RAY_TYPE_OCCLUSION 0
#endif
#ifndef RAY_TYPE_GI
#define RAY_TYPE_GI 1
#endif

[shader("closesthit")]
void main_closesthit(inout IrradiancePayload payload, in float2 barycentrics) {
    const uint primitive_id = InstanceID();
    const uint triangle_id = PrimitiveIndex();

    const PrimitiveDataGPU primitive = primitive_datas[primitive_id];

    const uint index = triangle_id * 3;

    const uint i0 = primitive.indices[index];
    const uint i1 = primitive.indices[index + 1];
    const uint i2 = primitive.indices[index + 2];

    const StandardVertexData v0 = primitive.vertex_data[i0];
    const StandardVertexData v1 = primitive.vertex_data[i1];
    const StandardVertexData v2 = primitive.vertex_data[i2];

    const StandardVertexData v = interpolate_vertex(v0, v1, v2, barycentrics);

    const float3 p0 = primitive.vertex_positions[i0];
    const float3 p1 = primitive.vertex_positions[i1];
    const float3 p2 = primitive.vertex_positions[i2];

    float3 b = float3(1.f - barycentrics.x - barycentrics.y, barycentrics.x, barycentrics.y);
    const float3 modelspace_vertex_position = b.x * p0 + b.y * p1 + b.z * p2;
    const float4 worldspace_vertex_position = mul(primitive.model, float4(modelspace_vertex_position, 1));

    const BasicPbrMaterialGpu material = *primitive.material;

    SurfaceInfo surface = (SurfaceInfo)0;
    surface.location = worldspace_vertex_position.xyz;

    // Base color
    Sampler2D base_color_texture = textures[material.base_color_texture_index];
    const float4 base_color_sample = base_color_texture.SampleLevel(v.texcoord, 0);
    surface.base_color = (base_color_sample * material.base_color_tint * unpackUnorm4x8ToHalf(v.color)).rgb;

    // Normal
    // TODO
    // float3x3 tbn = cotangent_frame(vertex.normal, vertex.viewspace_position, vertex.texcoord);
    // Sampler2D normals_texture = textures[material.normal_texture_index];
    // half3 normal_sample = (half3)normals_texture.SampleLevel(vertex.texcoord, 0).xyz * 2.h - 1.h;
    // half3 normal = mul(normal_sample, tbn);
    surface.normal = v.normal;

    // Data
    Sampler2D data_texture = textures[material.data_texture_index];
    half4 data_sample = (half4)data_texture.SampleLevel(v.texcoord, 0);
    const half4 tinted_data = data_sample * half4(0, (half)material.roughness_factor, (half)material.metalness_factor, 0);
    surface.roughness = tinted_data.g;
    surface.metalness = tinted_data.b;

    // Emission
    Sampler2D emission_texture = textures[material.emission_texture_index];
    half4 emission_sample = (half4)emission_texture.SampleLevel(v.texcoord, 0);
    surface.emission = (emission_sample * (half4)material.emission_factor).rgb;

    const half3 light_vector = normalize((half3)-sun_light.direction_and_tan_size.xyz);
    const half3 brdf_result = Fd(surface, light_vector, surface.normal);
    const half ndotl = (half)clamp(dot(light_vector, surface.normal), 0.h, 1.h);

    half shadow = 0;

    const uint2 noisetex_pixel = uint2(DispatchRaysIndex().xy) % 128;
    const float3 noise = normalize(noisetex[noisetex_pixel].xyz * 2.f - 1.f);

    if (ndotl > 0) {
        const float noise_size = sun_light.direction_and_tan_size.w;

        const float3 ray_direction = normalize(light_vector + noise * noise_size);

        RayDesc ray;
        ray.Origin = surface.location;
        ray.Direction = ray_direction;
        ray.TMin = 0.05f;
        ray.TMax = 100000.f;

        // 0 if we hit something, 1 if not
        uint payload = 1;

        TraceRay(
            rtas,
            RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_CULL_FRONT_FACING_TRIANGLES,
            0xFF,
            RAY_TYPE_OCCLUSION,
            1,
            0,
            ray,
            payload);

        if (payload > 0) {
            shadow = 1;
        }
    }

    payload.irradiance = brdf_result * sun_light.color.rgb * ndotl * shadow;

    payload.irradiance += surface.emission;

    payload.ray_distance = RayTCurrent();

    if (payload.remaining_bounces > 0) {
        float3 ray_direction = noise;
        if (dot(surface.normal, ray_direction) < 0) {
            ray_direction *= -1;
        }

        RayDesc bounce_ray;
        bounce_ray.Origin = surface.location;
        bounce_ray.Direction = ray_direction;
        bounce_ray.TMin = 0.05f;
        bounce_ray.TMax = 100000.f;

        // irradiance that comes in along the ray
        IrradiancePayload new_payload = (IrradiancePayload)0;
        new_payload.remaining_bounces = payload.remaining_bounces - 1;

        TraceRay(
            rtas,
            RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
            0xFF,
            RAY_TYPE_GI,
            1,
            0,
            bounce_ray,
            new_payload);

        const half3 bounce_brdf_result = brdf(surface, bounce_ray.Direction, surface.normal);
        const half bounce_ndotl = (half)clamp(dot(bounce_ray.Direction, surface.normal), 0.h, 1.h);

        const float3 bounce_radiance = bounce_ndotl * bounce_brdf_result * new_payload.irradiance;

        if (!any(isnan(bounce_radiance)) && !any(isinf(bounce_radiance))) {
            payload.irradiance += bounce_radiance;
        }
    }

    payload.ray_distance *= sign(dot(surface.normal, WorldRayDirection()));
}
#endif  // SAH_RT_GI
#endif // SAH_RT
