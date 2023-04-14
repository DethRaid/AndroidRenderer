#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference_uvec2 : enable

#include "shared/primitive_data.hpp"
#include "shared/basic_pbr_material.hpp"

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer PrimitiveDataBuffer {
    PrimitiveDataGPU primitive_datas[];
};

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer MaterialBuffer {
    BasicPbrMaterialGpu materials[];
};

layout(push_constant) uniform Constants {
    PrimitiveDataBuffer primitive_data_buffer;
    MaterialBuffer material_buffer;
    uint primitive_id;
};

layout(set = 1, binding = 0) uniform sampler2D textures[];

layout(location = 0) in mediump vec3 vertex_normal;
layout(location = 1) in mediump vec3 vertex_tangent;
layout(location = 2) in vec2 vertex_texcoord;
layout(location = 3) in mediump vec4 vertex_color;

layout(location = 0) out mediump vec4 gbuffer_base_color;
layout(location = 1) out mediump vec4 gbuffer_normal;
layout(location = 2) out mediump vec4 gbuffer_data;
layout(location = 3) out mediump vec4 gbuffer_emission;

void main() {
    PrimitiveDataGPU primitive_data = primitive_data_buffer.primitive_datas[primitive_id];
    BasicPbrMaterialGpu material = material_buffer.materials[primitive_data.data.x];

    // Base color
    mediump vec4 base_color_sample = texture(textures[nonuniformEXT(material.base_color_texture_index)], vertex_texcoord);
    mediump vec4 tinted_base_color = base_color_sample * material.base_color_tint * vertex_color;

    gbuffer_base_color = tinted_base_color;

    // Normals
    mediump vec3 bitangent = cross(vertex_tangent, vertex_normal);
    mediump mat3 tbn = transpose(mat3(
        vertex_tangent,
        bitangent,
        vertex_normal
    ));
    mediump vec3 normal_sample = texture(textures[nonuniformEXT(material.normal_texture_index)], vertex_texcoord).xyz * 2.0 - 1.0;
    mediump vec3 normal = tbn * normal_sample;
    gbuffer_normal = vec4(vertex_normal, 0.f);

    // Data
    mediump vec4 data_sample = texture(textures[nonuniformEXT(material.data_texture_index)], vertex_texcoord);
    mediump vec4 tinted_data = data_sample * vec4(0.f, material.roughness_factor, material.metalness_factor, 0.f);

    gbuffer_data = tinted_data;

    // Emission
    // TODO: Make sure this works well with my lighting model
    mediump vec4 emission_sample = texture(textures[nonuniformEXT(material.emission_texture_index)], vertex_texcoord);
    mediump vec4 tinted_emission = emission_sample * material.emission_factor;

    gbuffer_emission = tinted_emission;
}
