#pragma once

#include <volk.h>

#include "render/backend/texture_state.hpp"

VkAccessFlags to_access_mask(TextureState state);

VkImageLayout to_layout(TextureState state);

VkPipelineStageFlags to_stage_flags(TextureState state);

bool is_depth_format(VkFormat format);


