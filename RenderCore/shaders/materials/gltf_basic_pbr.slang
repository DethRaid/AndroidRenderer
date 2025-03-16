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

#if SAH_MAIN_VIEW
ConstantBuffer<ViewDataGPU> camera_data;
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

StructuredBuffer<PrimitiveDataGPU> primitive_datas;

StructuredBuffer<BasicPbrMaterialGpu> materials;

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
    half3 tangent;
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
    const float3 tangent_in,
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
    float4 viewspace_position = mul(camera_data.view, mul(data.model, float4(position_in, 1.f)));
    output.position = mul(camera_data.projection, viewspace_position);
    output.viewspace_position = viewspace_position.xyz;
#elif SAH_MULTIVIEW
    output.position = mul(world_to_ndc_matrices[view_id], mul(data.model, float4(position_in, 1.f)));
#endif

    output.texcoord = texcoord_in;
    output.color = color_in;

#if !SAH_DEPTH_ONLY
    output.normal = (half3)normalize(mul((half3x3)data.model, normal_in));
    output.tangent = (half3)normalize(mul((half3x3)data.model, tangent_in));
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
    BasicPbrMaterialGpu material = materials[primitive.material_id];

    GbufferOutput gbuffer = (GbufferOutput)0;

    float mip_bias = 0;
#if SAH_MAIN_VIEW
    mip_bias = camera_data.material_texture_mip_bias;
#endif

    // Base color
    Sampler2D base_color_texture = textures[material.base_color_texture_index];
    half4 base_color_sample = (half4)base_color_texture.SampleBias(vertex.texcoord, mip_bias);
    half4 tinted_base_color = base_color_sample * vertex.color * (half4)material.base_color_tint;

#if SAH_MASKED
    if (tinted_base_color.a < material.opacity_threshold) {
        discard;
    }
#endif  // SAH_MASKED

#if !SAH_DEPTH_ONLY
    gbuffer.base_color = tinted_base_color;

    // Normals
#if SAH_MAIN_VIEW
    float3x3 tbn = cotangent_frame(vertex.normal, vertex.viewspace_position, vertex.texcoord);
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

    const half3 brdf_result = Fd(surface, (half3)-sun_light.direction_and_size.xyz, surface.normal);

    RsmOutput output;
    output.flux = half4(brdf_result, 1);
    output.normal = half4(gbuffer.normal.xyz * 0.5h + 0.5h, 1);
    return output;
#endif
}

#else // SAH_RT

StandardVertexData interpolate_vertex(const StandardVertexData v0, const StandardVertexData v1, const StandardVertexData v2, const float2 bary)
{
    StandardVertexData v = (StandardVertexData)0;
    v.normal = v0.normal + bary.x * (v1.normal - v0.normal) + bary.y * (v2.normal - v0.normal);
    v.tangent = v0.tangent + bary.x * (v1.tangent - v0.tangent) + bary.y * (v2.tangent - v0.tangent);
    v.texcoord = v0.texcoord + bary.x * (v1.texcoord - v0.texcoord) + bary.y * (v2.texcoord - v0.texcoord);
    v.color = v0.color + (unorm float4)bary.xxxx * (v1.color - v0.color) + (unorm float4)bary.yyyy * (v2.color - v0.color);

    return v;
}

StructuredBuffer<float3> vertex_positions;
StructuredBuffer<StandardVertexData> vertex_data;
StructuredBuffer<uint> index_buffer;

#if SAH_RT_OCCLUSION
#if SAH_MASKED
[shader("anyhit")]
void main_anyhit(inout uint payload, in float2 barycentrics) {
    const uint primitive_id = InstanceID();
    const uint triangle_id = PrimitiveIndex();

    const uint index = triangle_id * 3;

    const uint i0 = index_buffer[index];
    const uint i1 = index_buffer[index + 1];    
    const uint i2 = index_buffer[index + 2];

    const StandardVertexData v0 = vertex_data[i0];
    const StandardVertexData v1 = vertex_data[i1];
    const StandardVertexData v2 = vertex_data[i2];

    const StandardVertexData v = interpolate_vertex(v0, v1, v2, barycentrics);

    const PrimitiveDataGPU primitive = primitive_datas[primitive_id];
    const BasicPbrMaterialGpu material = materials[primitive.material_id];

    Sampler2D base_color_texture = textures[material.base_color_texture_index];
    const float4 base_color_sample = base_color_texture.SampleLevel(v.texcoord, 0);
    const float4 base_color = base_color_sample * material.base_color_tint;

    if (base_color_sample.a < material.opacity_threshold) {
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
ConstantBuffer<SunLightConstants> sun_light;

#if SAH_MASKED
[shader("anyhit")]
void main_anyhit(inout uint payload, in float2 barycentrics) {
    const uint primitive_id = InstanceID();
    const uint triangle_id = PrimitiveIndex();

    const uint index = triangle_id * 3;

    const uint i0 = index_buffer[index];
    const uint i1 = index_buffer[index + 1];
    const uint i2 = index_buffer[index + 2];

    const StandardVertexData v0 = vertex_data[i0];
    const StandardVertexData v1 = vertex_data[i1];
    const StandardVertexData v2 = vertex_data[i2];

    const StandardVertexData v = interpolate_vertex(v0, v1, v2, barycentrics);

    const PrimitiveDataGPU primitive = primitive_datas[primitive_id];
    const BasicPbrMaterialGpu material = materials[primitive.material_id];

    Sampler2D base_color_texture = textures[material.base_color_texture_index];
    const float4 base_color_sample = base_color_texture.SampleLevel(v.texcoord, 0);
    const float4 base_color = base_color_sample * material.base_color_tint;

    if (base_color_sample.a < material.opacity_threshold) {
        IgnoreHit();
    }
}
#endif // SAH_MASKED

[shader("closesthit")]
void main_closesthit(inout half3 payload, in float2 barycentrics) {
    const uint primitive_id = InstanceID();
    const uint triangle_id = PrimitiveIndex();

    const uint index = triangle_id * 3;

    const uint i0 = index_buffer[index];
    const uint i1 = index_buffer[index + 1];
    const uint i2 = index_buffer[index + 2];

    const StandardVertexData v0 = vertex_data[i0];
    const StandardVertexData v1 = vertex_data[i1];
    const StandardVertexData v2 = vertex_data[i2];

    const StandardVertexData vertex = interpolate_vertex(v0, v1, v2, barycentrics);

    const PrimitiveDataGPU primitive = primitive_datas[primitive_id];
    const BasicPbrMaterialGpu material = materials[primitive.material_id];

    GbufferOutput gbuffer = (GbufferOutput)0;

    // Base color
    Sampler2D base_color_texture = textures[material.base_color_texture_index];
    const float4 base_color_sample = base_color_texture.SampleLevel(vertex.texcoord, 0);
    gbuffer.base_color = base_color_sample * material.base_color_tint;

    // Normal
    // TODO
    // float3x3 tbn = cotangent_frame(vertex.normal, vertex.viewspace_position, vertex.texcoord);
    // Sampler2D normals_texture = textures[material.normal_texture_index];
    // half3 normal_sample = (half3)normals_texture.SampleLevel(vertex.texcoord, 0).xyz * 2.h - 1.h;
    // half3 normal = mul(normal_sample, tbn);
    gbuffer.normal = half4(vertex.normal, 0);

    // Data
    Sampler2D data_texture = textures[material.data_texture_index];
    half4 data_sample = (half4)data_texture.SampleLevel(vertex.texcoord, 0);
    gbuffer.data = data_sample * half4(0, (half)material.roughness_factor, (half)material.metalness_factor, 0);

    // Emission
    Sampler2D emission_texture = textures[material.emission_texture_index];
    half4 emission_sample = (half4)emission_texture.SampleLevel(vertex.texcoord, 0);
    gbuffer.emission = emission_sample * (half4)material.emission_factor;

    SurfaceInfo surface = (SurfaceInfo)0;
    surface.base_color = gbuffer.base_color.rgb;
    surface.normal = gbuffer.normal.xyz;
    surface.metalness = gbuffer.data.b;
    surface.roughness = gbuffer.data.g;

    // TODO: RT sun occlusion, proper pathtracing
    const half3 brdf_result = Fd(surface, (half3)-sun_light.direction_and_size.xyz, surface.normal);

    payload = brdf_result;
}
#endif  // SAH_RT_GI
#endif // SAH_RT
