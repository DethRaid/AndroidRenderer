#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference_uvec2 : enable

#include "common/brdf.glsl"
#include "shared/sun_light_constants.hpp"
#include "shared/basic_pbr_material.hpp"
#include "shared/primitive_data.hpp"

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer PrimitiveDataBuffer {
    PrimitiveDataGPU primitive_datas[];
};

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer PrimitiveIdBuffer {
    uint primitive_ids[];
};

layout(push_constant) uniform Constants {
    PrimitiveDataBuffer primitive_data_buffer;
    PrimitiveIdBuffer primitive_id_buffer;
    uint cascade_index;
};

layout(set = 0, binding = 1) uniform SunLightBuffer {
    SunLightConstants sun;
};

layout(set = 1, binding = 0) uniform sampler2D textures[];

layout(location = 0) in mediump vec3 vertex_normal;
layout(location = 1) in mediump vec3 vertex_tangent;
layout(location = 2) in vec2 vertex_texcoord;
layout(location = 3) in mediump vec4 vertex_color;
layout(location = 4) flat in uint primitive_id;

layout(location = 0) out mediump vec4 rsm_flux;
layout(location = 1) out mediump vec4 rsm_normal;

void main() {
    PrimitiveDataGPU primitive = primitive_data_buffer.primitive_datas[primitive_id];
    BasicPbrMaterialGpu material = primitive.material_id.material;

    // Base color
    mediump vec4 base_color_sample = texture(textures[material.base_color_texture_index], vertex_texcoord);
    mediump vec4 tinted_base_color = base_color_sample * material.base_color_tint * vertex_color;

    mediump vec4 data_sample = texture(textures[nonuniformEXT(material.data_texture_index)], vertex_texcoord);
    mediump vec4 tinted_data = data_sample * vec4(0.f, material.roughness_factor, material.metalness_factor, 0.f);

    SurfaceInfo surface;
    surface.base_color = tinted_base_color.rgb;
    surface.normal = vertex_normal;
    surface.metalness = tinted_data.b;
    surface.roughness = tinted_data.g;
    
    // We use the normal as the view vector because we want the light reflected directly away from the surface
    const mediump vec3 brdf_result = Fd(surface, -sun.direction_and_size.xyz, surface.normal);

    rsm_flux = vec4(brdf_result, 1.f);

    // Normals
    // TODO: Normalmapping
    rsm_normal = vec4(vertex_normal * 0.5f + 0.5f, 0.f);
}
