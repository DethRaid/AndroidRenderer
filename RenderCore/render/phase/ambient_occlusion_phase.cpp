#include "ambient_occlusion_phase.hpp"

#include <ffx_api/vk/ffx_api_vk.hpp>
#include <FidelityFX/host/backends/vk/ffx_vk.h>

#include "console/cvars.hpp"
#include "render/scene_view.hpp"
#include "render/backend/render_backend.hpp"

static AutoCVar_Enum<FfxCacaoQuality> cvar_cacao_quality{
    "r.CACAO.Quality", "Quality of CACAO", FFX_CACAO_QUALITY_HIGHEST
};

AmbientOcclusionPhase::AmbientOcclusionPhase() {
    const auto& backend = RenderBackend::get();
    auto vk_desc = ffx::CreateBackendVKDesc{};
    vk_desc.vkDevice = backend.get_device();
    vk_desc.vkPhysicalDevice = backend.get_physical_device();
    vk_desc.vkDeviceProcAddr = vkGetDeviceProcAddr;

    ffx::CreateContext(ffx, nullptr, vk_desc);

    ffx_device = ffxGetDeviceVK(reinterpret_cast<VkDeviceContext*>(&vk_desc));

    const auto scratch_memory_size = ffxGetScratchMemorySizeVK(backend.get_physical_device(), FFX_CACAO_CONTEXT_COUNT);
    auto scratch_memory = std::vector<uint8_t>(scratch_memory_size);
    ffxGetInterfaceVK(
        &ffx_interface,
        ffx_device,
        scratch_memory.data(),
        scratch_memory.size(),
        FFX_CACAO_CONTEXT_COUNT);
}

AmbientOcclusionPhase::~AmbientOcclusionPhase() {
    ffxCacaoContextDestroy(&context);

    ffx::DestroyContext(ffx);
}

void AmbientOcclusionPhase::generate_ao(
    RenderGraph& graph, const SceneTransform& view, TextureHandle gbuffer_normals, TextureHandle gbuffer_depth,
    TextureHandle ao_out
) {
    ZoneScoped;

    if(!has_context) {
        FfxCacaoContextDescription description = {};
        description.backendInterface = ffx_interface;
        description.width = ao_out->create_info.extent.width;
        description.height = ao_out->create_info.extent.height;
        description.useDownsampledSsao = false;
        const auto error_code = ffxCacaoContextCreate(&context, &description);
        if(error_code != FFX_OK) {
            spdlog::error("Could not initialize FFX CACAO context");
            return;
        }

        has_context = true;
    }

    auto ffx_depth = ffxGetResourceVK(
        gbuffer_depth->image,
        {
            .type = FFX_RESOURCE_TYPE_TEXTURE2D,
            .format = FFX_SURFACE_FORMAT_R32_FLOAT,
            .width = gbuffer_depth->create_info.extent.depth,
            .height = gbuffer_depth->create_info.extent.height,
            .depth = 1,
            .mipCount = 1,
            .flags = FFX_RESOURCE_FLAGS_NONE,
            .usage = FFX_RESOURCE_USAGE_READ_ONLY
        },
        nullptr);
    auto ffx_normals = ffxGetResourceVK(
        gbuffer_normals->image,
        {
            .type = FFX_RESOURCE_TYPE_TEXTURE2D,
            .format = FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT,
            .width = gbuffer_normals->create_info.extent.depth,
            .height = gbuffer_normals->create_info.extent.height,
            .depth = 1,
            .mipCount = 1,
            .flags = FFX_RESOURCE_FLAGS_NONE,
            .usage = FFX_RESOURCE_USAGE_READ_ONLY
        },
        nullptr);
    auto ffx_ao = ffxGetResourceVK(
        ao_out->image,
        {
            .type = FFX_RESOURCE_TYPE_TEXTURE2D,
            .format = FFX_SURFACE_FORMAT_R32_FLOAT,
            .width = ao_out->create_info.extent.depth,
            .height = ao_out->create_info.extent.height,
            .depth = 1,
            .mipCount = 1,
            .flags = FFX_RESOURCE_FLAGS_NONE,
            .usage = FFX_RESOURCE_USAGE_READ_ONLY
        },
        nullptr);

    graph.add_pass(
        {
            .name = "CACAO",
            .textures = {
                {
                    gbuffer_normals,
                    {
                        .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, .access = VK_ACCESS_2_SHADER_READ_BIT,
                        .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    }
                },
                {
                    gbuffer_depth,
                    {
                        .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, .access = VK_ACCESS_2_SHADER_READ_BIT,
                        .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    }
                },
                {
                    ao_out,
                    {
                        .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, .access = VK_ACCESS_2_SHADER_READ_BIT,
                        .layout = VK_IMAGE_LAYOUT_GENERAL
                    }
                },
            },
            .execute = [=, this](CommandBuffer& commands) {
                const auto cacao_settings = FfxCacaoSettings{
                    .radius = 0.5,
                    .shadowMultiplier = 1.f,
                    .shadowPower = 1.f,
                    .shadowClamp = 0.f,
                    .horizonAngleThreshold = 0.f,
                    .fadeOutFrom = 1000.f,
                    .fadeOutTo = 1100.f,
                    .qualityLevel = cvar_cacao_quality.Get(),
                    .adaptiveQualityLimit = 1.0,
                    .blurPassCount = 1,
                    .sharpness = 1.f,
                    .temporalSupersamplingAngleOffset = 0.f,
                    .temporalSupersamplingRadiusOffset = 0.f,
                    .detailShadowStrength = 1.f,
                    .generateNormals = false,
                    .bilateralSigmaSquared = 0.f,
                    .bilateralSimilarityDistanceSigma = 0.f
                };
                FfxErrorCode errorCode = ffxCacaoUpdateSettings(&context, &cacao_settings, false);

                auto ffx_cmds = ffxGetCommandListVK(commands.get_vk_commands());

                FfxFloat32x4x4 projection_matrix = {};
                std::memcpy(&projection_matrix, &view.get_gpu_data().projection, sizeof(glm::mat4));

                FfxFloat32x4x4 normals_to_view = {};
                std::memcpy(&normals_to_view, &view.get_gpu_data().view, sizeof(glm::mat4));

                const auto desc = FfxCacaoDispatchDescription{
                    .commandList = ffx_cmds,
                    .depthBuffer = ffx_depth,
                    .normalBuffer = ffx_normals,
                    .outputBuffer = ffx_ao,
                    .proj = &projection_matrix,
                    .normalsToView = &normals_to_view,
                    .normalUnpackMul = 1,
                    .normalUnpackAdd = 0
                };

                ffxCacaoContextDispatch(&context, &desc);
            }
        });

}
