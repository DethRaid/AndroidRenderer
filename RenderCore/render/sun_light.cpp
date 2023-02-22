#include "sun_light.hpp"

#include "render/backend/render_backend.hpp"
#include "render/backend/resource_allocator.hpp"
#include "render/backend/command_buffer.hpp"
#include "render/scene_view.hpp"
#include "console/cvars.hpp"
#include "core/system_interface.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/matrix_clip_space.hpp"

static std::shared_ptr<spdlog::logger> logger;

SunLight::SunLight(RenderBackend& backend) : allocator{backend.get_global_allocator()} {
    logger = SystemInterface::get().get_logger("SunLight");

    sun_buffer = allocator.create_buffer(
        "Sun Constant Buffer", sizeof(SunLightConstants),
        BufferUsage::UniformBuffer
    );

    pipeline = backend.begin_building_pipeline("Sun Light")
                      .set_vertex_shader("shaders/common/fullscreen.vert.spv")
                      .set_fragment_shader("shaders/lighting/directional_light.frag.spv")
                      .set_depth_state(
                          DepthStencilState{
                              .enable_depth_test = false,
                              .enable_depth_write = false,
                          }
                      )
                      .set_blend_state(
                          0, {
                              .blendEnable = VK_TRUE,
                              .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR,
                              .dstColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR,
                              .colorBlendOp = VK_BLEND_OP_ADD,
                              .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
                          }
                      )
                      .build();

    
}

void SunLight::update_shadow_cascades(SceneView& view) {
    const auto num_cascades = static_cast<uint32_t>(*CVarSystem::Get()->GetIntCVar("r.Shadow.NumCascades"));
    const auto max_shadow_distance = static_cast<float>(*CVarSystem::Get()->GetFloatCVar("r.Shadow.Distance"));
    const auto cascade_split_lambda = static_cast<float>(*CVarSystem::Get()->GetFloatCVar("r.Shadow.CascadeSplitLambda"));

    // Shadow frustum fitting code based on
    // https://github.com/SaschaWillems/Vulkan/blob/master/examples/shadowmappingcascade/shadowmappingcascade.cpp#L637,
    // adapted for infinite projection

    /*
     * Algorithm:
     * - Transform frustum corners from NDC to worldspace
     *  May need to use a z of 0.5 for the far points, because infinite projection
     * - Get the direction vectors in the viewspace z for each frustum corner
     * - Multiply by each cascade's begin and end distance to get the eight points of the frustum that the cascade must cover
     * - Transform points into lightspace, calculate min and max x y and max z
     * - Fit shadow frustum to those bounds, adjusting frustum's view matrix to keep the frustum centered
     */

    const auto z_near = view.get_near();
    const auto clip_range = z_near + max_shadow_distance;
    const auto ratio = clip_range / z_near;

    auto cascade_splits = std::vector<float>{};
    cascade_splits.resize(num_cascades);

    // Calculate split depths based on view camera frustum
    // Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
    // Also based on Sasha Willem's Vulkan examples
    for (uint32_t i = 0; i < num_cascades; i++) {
        float p = (i + 1) / static_cast<float>(num_cascades);
        float log = z_near * std::pow(ratio, p);
        float uniform = z_near + max_shadow_distance * p;
        float d = cascade_split_lambda * (log - uniform) + uniform;
        cascade_splits[i] = (d - z_near) / clip_range;
    }

    auto last_split_distance = z_near;
    for (auto i = 0u; i < num_cascades; i++) {
        const auto split_distance = cascade_splits[i];

        auto frustum_corners = std::array{
            glm::vec3{-1.0f,  1.0f, -1.0f},
            glm::vec3{ 1.0f,  1.0f, -1.0f},
            glm::vec3{ 1.0f, -1.0f, -1.0f},
            glm::vec3{-1.0f, -1.0f, -1.0f},
            glm::vec3{-1.0f,  1.0f,  1.0f},
            glm::vec3{ 1.0f,  1.0f,  1.0f},
            glm::vec3{ 1.0f, -1.0f,  1.0f},
            glm::vec3{-1.0f, -1.0f,  1.0f},
        };

        // Construct a new projection matrix for the region of the frustum this cascade
        // zNear and zFar intentionally reversed because reverse-z. Probably doesn't mathematically matter, but it
        // keeps the contentions consistent
        const auto projection_matrix = glm::perspectiveFov(
            view.get_fov(),
            view.get_aspect_ratio(),
            1.f,
            last_split_distance * max_shadow_distance,
            split_distance * max_shadow_distance
        );

        const auto inverse_camera = glm::inverse(projection_matrix * view.get_gpu_data().view);

        for (auto& corner : frustum_corners) {
            const auto transformed_corner = inverse_camera * glm::vec4{corner, 1.f};
            corner = glm::vec3{transformed_corner} / transformed_corner.w;
        }

        // Get frustum center
        auto frustum_center = glm::vec3{0};
        for (const auto& corner : frustum_corners) {
            frustum_center += corner;
        }
        frustum_center /= frustum_corners.size();

        // Fit a sphere to the frustum
        float radius = 0.f;
        for (const auto& corner : frustum_corners) {
            const auto distance = glm::length(corner - frustum_center);
            radius = glm::max(radius, distance);
        }

        // Snap to 16 to avoid texel swimming
        radius = std::ceil(radius * 16.f) / 16.f;

        // Shadow cascade frustum
        const auto light_dir = glm::normalize(glm::vec3{constants.direction_and_size});

        // TODO: Properly find the top of the scene. Maybe the top of the bounding boxes of the objects that are potentially in the shadow frustum?
        const auto max_height = std::max(32.f, radius);
                
        const auto light_view_matrix = glm::lookAt(
            frustum_center - light_dir * max_height, frustum_center, glm::vec3{0.f, 1.f, 0.f}
        );
        const auto light_projection_matrix = glm::ortho(-radius, radius, -radius, max_height, 0.f, max_height + radius);

        // Store split distance and matrix in cascade
        constants.data[i] = glm::vec4{};
        constants.data[i].x = split_distance * clip_range * -1;
        constants.cascade_matrices[i] = light_projection_matrix * light_view_matrix;
        constants.cascade_inverse_matrices[i] = glm::inverse(constants.cascade_matrices[i]);

        last_split_distance = cascade_splits[i];
    }

    const auto csm_resolution = static_cast<uint32_t>(*CVarSystem::Get()->GetIntCVar("r.Shadow.CascadeResolution"));
    constants.csm_resolution.x = csm_resolution;
    constants.csm_resolution.y = csm_resolution;

    sun_buffer_dirty = true;
}

void SunLight::set_direction(const glm::vec3& direction) {
    constants.direction_and_size = glm::vec4{glm::normalize(direction), 1.f};
    sun_buffer_dirty = true;
}

void SunLight::set_color(const glm::vec4 color) {
    constants.color = color;
    sun_buffer_dirty = true;
}

void SunLight::update_buffer(CommandBuffer& commands) {
    // Write the data to the buffer
    // This is NOT safe. We'll probably write data while the GPU is reading data
    // A better solution might use virtual resources in the frontend and assign real resources
    // just-in-time. That'd solve sync without making the frontend care about frames. We could also
    // just have the frontend care about frames...

    if (sun_buffer_dirty) {
        commands.update_buffer(sun_buffer, constants);

        sun_buffer_dirty = false;
    }
}

BufferHandle SunLight::get_constant_buffer() const {
    return sun_buffer;
}

Pipeline& SunLight::get_pipeline() {
    return pipeline;
}
