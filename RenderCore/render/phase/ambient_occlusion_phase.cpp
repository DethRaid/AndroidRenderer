#include "ambient_occlusion_phase.hpp"

#if defined(SAH_USE_FFX_CACAO) && SAH_USE_FFX_CACAO
#include <ffx_api/vk/ffx_api_vk.h>
#include <FidelityFX/host/backends/vk/ffx_vk.h>
#endif

#include "console/cvars.hpp"
#include "core/string_conversion.hpp"
#include "render/scene_view.hpp"
#include "render/backend/render_backend.hpp"

static AutoCVar_Int cvar_cacao_enabled{
    "r.CACAO.Enabled", "Whether to use CACAO or not", true
};

#if defined(SAH_USE_FFX_CACAO) && SAH_USE_FFX_CACAO
static AutoCVar_Enum<FfxCacaoQuality> cvar_cacao_quality{
    "r.CACAO.Quality", "Quality of CACAO", FFX_CACAO_QUALITY_HIGHEST
};

static std::string to_string(const ffxReturnCode_t code) {
    switch(code) {
    case FFX_API_RETURN_OK:
        return "The operation was successful";

    case FFX_API_RETURN_ERROR:
        return "An error occurred that is not further specified.";

    case FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE:
        return
            "The structure type given was not recognized for the function or context with which it was used. This is likely a programming error.";

    case FFX_API_RETURN_ERROR_RUNTIME_ERROR:
        return "The underlying runtime (e.g. D3D12, Vulkan) or effect returned an error code.";

    case FFX_API_RETURN_NO_PROVIDER:
        return "No provider was found for the given structure type. This is likely a programming error.";

    case FFX_API_RETURN_ERROR_MEMORY:
        return "A memory allocation failed";

    case FFX_API_RETURN_ERROR_PARAMETER:
        return "A parameter was invalid, e.g. a null pointer, empty resource or out-of-bounds enum value.";

    case FFX_ERROR_INVALID_POINTER:
        return "The operation failed due to an invalid pointer.";

    case FFX_ERROR_INVALID_ALIGNMENT:
        return "The operation failed due to an invalid alignment.";

    case FFX_ERROR_INVALID_SIZE:
        return "The operation failed due to an invalid size.";

    case FFX_EOF:
        return "The end of the file was encountered.";

    case FFX_ERROR_INVALID_PATH:
        return "The operation failed because the specified path was invalid.";

    case FFX_ERROR_EOF:
        return "The operation failed because end of file was reached.";

    case FFX_ERROR_MALFORMED_DATA:
        return "The operation failed because of some malformed data.";

    case FFX_ERROR_OUT_OF_MEMORY:
        return "The operation failed because it ran out of memory.";

    case FFX_ERROR_INCOMPLETE_INTERFACE:
        return "The operation failed because the interface was not fully configured.";

    case FFX_ERROR_INVALID_ENUM:
        return "The operation failed because of an invalid enumeration value.";

    case FFX_ERROR_INVALID_ARGUMENT:
        return "The operation failed because an argument was invalid.";

    case FFX_ERROR_OUT_OF_RANGE:
        return "The operation failed because a value was out of range.";

    case FFX_ERROR_NULL_DEVICE:
        return "The operation failed because a device was null.";

    case FFX_ERROR_BACKEND_API_ERROR:
        return "The operation failed because the backend API returned an error code.";

    case FFX_ERROR_INSUFFICIENT_MEMORY:
        return "The operation failed because there was not enough memory.";

    case FFX_ERROR_INVALID_VERSION:
        return "The operation failed because the wrong backend was linked.";

    default:
        return "Unknown error";
    }
}
#endif

AmbientOcclusionPhase::AmbientOcclusionPhase() {
#if defined(SAH_USE_FFX_CACAO) && SAH_USE_FFX_CACAO
    const auto& backend = RenderBackend::get();
    // auto vk_desc = ffxCreateBackendVKDesc{
    //     .header = {.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_VK},
    //     .vkDevice = backend.get_device(),
    //     .vkPhysicalDevice = backend.get_physical_device(),
    //     .vkDeviceProcAddr = vkGetDeviceProcAddr,
    // };
    // 
    // const auto result = ffxCreateContext(&ffx, &vk_desc.header, nullptr);
    // if(result != FFX_API_RETURN_OK) {
    //     const auto error_message = to_string(result);
    //     throw std::runtime_error{
    //         fmt::format("Could not create FFX context! {} (error code {})", error_message, result)
    //     };
    // }

    auto device_context = VkDeviceContext{
        .vkDevice = backend.get_device(),
        .vkPhysicalDevice = backend.get_physical_device(),
        .vkDeviceProcAddr = vkGetDeviceProcAddr
    };
    ffx_device = ffxGetDeviceVK(&device_context);
    if(ffx_device == nullptr) {
        throw std::runtime_error{"Could not get VK device!"};
    }

    ffx_interface.scratchBuffer = nullptr;

    // SDKWrapper::ffxGetScratchMemorySize(FFX_CACAO_CONTEXT_COUNT * 2);

    const auto scratch_memory_size = ffxGetScratchMemorySizeVK(
        backend.get_physical_device(),
        FFX_CACAO_CONTEXT_COUNT * 2);
    auto scratch_memory = calloc(scratch_memory_size, 1u);
    auto result = ffxGetInterfaceVK(
        &ffx_interface,
        ffx_device,
        scratch_memory,
        scratch_memory_size,
        FFX_CACAO_CONTEXT_COUNT * 2);

    if(result != FFX_API_RETURN_OK) {
        const auto error_string = to_string(result);
        throw std::runtime_error{
            fmt::format("Could not get the FFX VK interface: {} (Error code {})", error_string, result)
        };
    }
#endif
}

AmbientOcclusionPhase::~AmbientOcclusionPhase() {
#if defined(SAH_USE_FFX_CACAO) && SAH_USE_FFX_CACAO
    if(has_context) {
        ffxCacaoContextDestroy(&context);
    }

    // ffxDestroyContext(&ffx, nullptr);
#endif
}

void AmbientOcclusionPhase::generate_ao(
    RenderGraph& graph, const SceneTransform& view, TextureHandle gbuffer_normals, TextureHandle gbuffer_depth,
    TextureHandle ao_out
) {
    if(cvar_cacao_enabled.Get() == 0) {
        return;
    }

#if defined(SAH_USE_FFX_CACAO) && SAH_USE_FFX_CACAO
    ZoneScoped;

    if(!has_context) {
        FfxCacaoContextDescription description = {};
        description.backendInterface = ffx_interface;
        description.width = ao_out->create_info.extent.width;
        description.height = ao_out->create_info.extent.height;
        description.useDownsampledSsao = false;
        const auto error_code = ffxCacaoContextCreate(&context, &description);
        if(error_code != FFX_OK) {
            spdlog::error(
                "Could not initialize FFX CACAO context: {} (error code {})",
                to_string(error_code),
                error_code);
            return;
        }

        has_context = true;
    }

    if(stinky_depth == nullptr) {
        auto& allocator = RenderBackend::get().get_global_allocator();
        stinky_depth = allocator.create_texture(
            "R32F Depth Meme",
            {
                .format = VK_FORMAT_R32_SFLOAT,
                .resolution = {gbuffer_depth->create_info.extent.width, gbuffer_depth->create_info.extent.height},
                .num_mips = gbuffer_depth->create_info.mipLevels,
                .usage = TextureUsage::StorageImage
            });
    }

    graph.add_copy_pass(
        ImageCopyPass{
            .name = "Copy D32 to R32 lmao",
            .dst = stinky_depth,
            .src = gbuffer_depth
        });

    const auto stinky_depth_name = to_wstring(stinky_depth->name);
    auto ffx_depth = ffxGetResourceVK(
        stinky_depth->image,
        {
            .type = FFX_RESOURCE_TYPE_TEXTURE2D,
            .format = FFX_SURFACE_FORMAT_R32_FLOAT,
            .width = stinky_depth->create_info.extent.depth,
            .height = stinky_depth->create_info.extent.height,
            .depth = 1,
            .mipCount = 1,
            .flags = FFX_RESOURCE_FLAGS_NONE,
            .usage = FFX_RESOURCE_USAGE_READ_ONLY
        },
        stinky_depth_name.c_str());
    const auto stinky_normals_name = to_wstring(gbuffer_normals->name);
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
        stinky_normals_name.c_str());
    const auto stinky_ao_name = to_wstring(ao_out->name);
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
            .usage = FFX_RESOURCE_USAGE_UAV
        },
        stinky_ao_name.c_str());

    graph.add_pass(
        {
            .name = "CACAO",
            .textures = {
                {
                    .texture = gbuffer_normals,
                    .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .access = VK_ACCESS_2_SHADER_READ_BIT,
                    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                },
                {
                    .texture = stinky_depth,
                    .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .access = VK_ACCESS_2_SHADER_READ_BIT,
                    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                },
                {
                    .texture = ao_out,
                    .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .access = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
                    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
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
                FfxErrorCode errorCode = ffxCacaoUpdateSettings(&context, &FFX_CACAO_DEFAULT_SETTINGS, false);

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

    graph.set_resource_usage(
        {
            .texture = ao_out,
            .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .access = VK_ACCESS_2_SHADER_WRITE_BIT,
            .layout = VK_IMAGE_LAYOUT_GENERAL
        });
#else
    graph.add_render_pass({
        .name = "Clear AO",
        .color_attachments = {RenderingAttachmentInfo{
            .image = ao_out,
            .load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .clear_value = {.color = {.float32 = {1, 1, 1, 1}}}}
            },
        .execute = [](CommandBuffer&) {}
    });
#endif
}
