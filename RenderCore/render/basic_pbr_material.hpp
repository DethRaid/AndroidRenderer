#ifndef SAHRENDERER_BASIC_PBR_MATERIAL_HPP
#define SAHRENDERER_BASIC_PBR_MATERIAL_HPP

#include <filesystem>

#include "render/backend/handles.hpp"
#include "shared/basic_pbr_material.hpp"

static_assert(sizeof(BasicPbrMaterialGpu) % 64 == 0);

enum class TransparencyMode {
    Solid,
    Cutout,
    Translucent
};

struct PipelineData {
    std::filesystem::path vertex_shader_path;
    std::filesystem::path fragment_shader_path;
};

/**
 * Basic PBR material, based on glTF PBR metallic roughness
 *
 * Contains data for all the material features, even if a particular model doesn't use them. We set
 * those members to a sensible default - base color and metallic roughness textures are pure white,
 * normal texture is (0.5, 0.5, 1.0), emission texture is pure black
 *
 * The material storage fills out the Vulkan objects in here when you add a material. You need only
 * set the other members
 */
struct BasicPbrMaterial {
    std::string name;

    TransparencyMode transparency_mode;

    bool double_sided;

    bool front_face_ccw;

    TextureHandle base_color_texture;
    VkSampler base_color_sampler;

    TextureHandle normal_texture;
    VkSampler normal_sampler;

    TextureHandle metallic_roughness_texture;
    VkSampler metallic_roughness_sampler;

    TextureHandle emission_texture;
    VkSampler emission_sampler;

    VkDescriptorSet descriptor_set;

    BasicPbrMaterialGpu gpu_data;

    VkPipelineColorBlendAttachmentState blend_state;
};

#endif //SAHRENDERER_BASIC_PBR_MATERIAL_HPP
