#include "utils.hpp"

#include <stdexcept>

VkAccessFlags to_access_mask(const TextureState state) {
    switch (state) {
    case TextureState::ColorWrite:
        return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    case TextureState::DepthReadWrite:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    case TextureState::InputAttachment:
        return VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

    case TextureState::ColorRead:
        return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    case TextureState::DepthRead:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

    case TextureState::VertexShaderRead:
        [[fallthrough]];
    case TextureState::FragmentShaderRead:
        return VK_ACCESS_SHADER_READ_BIT;

    case TextureState::ShaderWrite:
        return VK_ACCESS_SHADER_WRITE_BIT;

    case TextureState::TransferSource:
        return VK_ACCESS_TRANSFER_READ_BIT;

    case TextureState::TransferDestination:
        return VK_ACCESS_TRANSFER_WRITE_BIT;

    default:
        throw std::runtime_error{"Unsupported texture state"};
    }
}

VkImageLayout to_layout(const TextureState state) {
    switch (state) {
    case TextureState::ColorWrite:
        [[fallthrough]];
    case TextureState::InputAttachment:
        [[fallthrough]];
    case TextureState::ColorRead:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    case TextureState::DepthReadWrite:
        [[fallthrough]];
    case TextureState::DepthRead:
        return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

    case TextureState::VertexShaderRead:
        [[fallthrough]];
    case TextureState::FragmentShaderRead:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    case TextureState::ShaderWrite:
        return VK_IMAGE_LAYOUT_GENERAL;

    case TextureState::TransferSource:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    case TextureState::TransferDestination:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    default:
        throw std::runtime_error{"Unsupported texture state"};
    }
}

VkPipelineStageFlags to_stage_flags(const TextureState state) {
    switch (state) {
    case TextureState::ColorWrite:
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    case TextureState::DepthRead:
        [[fallthrough]];
    case TextureState::DepthReadWrite:
        return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

    case TextureState::InputAttachment:
        // Possibly incorrect, probably not incorrect in this project
        return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    case TextureState::ColorRead:
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    case TextureState::VertexShaderRead:
        return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;

    case TextureState::FragmentShaderRead:
        return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    case TextureState::ShaderWrite:
        // Overly coarse. A more fully featured render graph would have hints about which shader stage needs this
        return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    case TextureState::TransferSource:
        [[fallthrough]];
    case TextureState::TransferDestination:
        return VK_PIPELINE_STAGE_TRANSFER_BIT;

    default:
        throw std::runtime_error{"Unsupported texture state"};
    }
}

bool is_depth_format(const VkFormat format) {
    return format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_X8_D24_UNORM_PACK32 || format == VK_FORMAT_D32_SFLOAT ||
        format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT ||
        format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}
