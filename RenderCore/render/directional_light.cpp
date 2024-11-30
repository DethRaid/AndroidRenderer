#include "directional_light.hpp"

#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include "render/backend/render_backend.hpp"
#include "render/backend/resource_allocator.hpp"
#include "render/backend/command_buffer.hpp"
#include "render/scene_view.hpp"
#include "console/cvars.hpp"
#include "core/system_interface.hpp"

static std::shared_ptr<spdlog::logger> logger;

enum class SunShadowMode {
    Off,
    CSM,
    RayQuery,
    RayPipeline
};

static AutoCVar_Enum cvar_sun_shadow_mode{
    "r.Shadow.SunShadowMode",
    "How to calculate shadows for the sun.\n\t0 = off\n\t1 = Cascade Shadow Maps\n\t2 = Hardware ray queries\n\t3 = Hardware ray pipelines",
    SunShadowMode::RayQuery
};

static auto cvar_num_shadow_cascades = AutoCVar_Int{"r.Shadow.NumCascades", "Number of shadow cascades", 4};

static auto cvar_shadow_cascade_resolution = AutoCVar_Int{
    "r.Shadow.CascadeResolution",
    "Resolution of one cascade in the shadowmap", 1024
};

static auto cvar_max_shadow_distance = AutoCVar_Float{"r.Shadow.Distance", "Maximum distance of shadows", 128};

static auto cvar_shadow_cascade_split_lambda = AutoCVar_Float{
    "r.Shadow.CascadeSplitLambda",
    "Factor to use when calculating shadow cascade splits", 0.95
};

DirectionalLight::DirectionalLight() {
    logger = SystemInterface::get().get_logger("SunLight");

    auto& allocator = RenderBackend::get().get_global_allocator();
    sun_buffer = allocator.create_buffer(
        "Sun Constant Buffer",
        sizeof(SunLightConstants),
        BufferUsage::UniformBuffer
    );

    auto& backend = RenderBackend::get();
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
                          0,
                          {
                              .blendEnable = VK_TRUE,
                              .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR,
                              .dstColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR,
                              .colorBlendOp = VK_BLEND_OP_ADD,
                              .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
                          }
                      )
                      .build();

    shadowmap_handle = allocator.create_texture(
        "Dummy directional shadowmap",
        VK_FORMAT_D16_UNORM,
        glm::uvec2{
            8,
            8
        },
        1,
        TextureUsage::RenderTarget,
        static_cast<uint32_t>(cvar_num_shadow_cascades.Get())
    );

}

void DirectionalLight::update_shadow_cascades(const SceneTransform& view) {
    if(cvar_sun_shadow_mode.Get() != SunShadowMode::CSM) {
        return;
    }

    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();

    if(has_dummy_shadowmap  && shadowmap_handle != TextureHandle::None) {
        allocator.destroy_texture(shadowmap_handle);
        shadowmap_handle = TextureHandle::None;
    }

    if (shadowmap_handle == TextureHandle::None) {
        shadowmap_handle = allocator.create_texture(
            "Sun shadowmap",
            VK_FORMAT_D16_UNORM,
            glm::uvec2{
                cvar_shadow_cascade_resolution.Get(),
                cvar_shadow_cascade_resolution.Get()
            },
            1,
            TextureUsage::RenderTarget,
            cvar_num_shadow_cascades.Get()
        );
    }

    const auto num_cascades = static_cast<uint32_t>(cvar_num_shadow_cascades.Get());
    const auto max_shadow_distance = static_cast<float>(cvar_max_shadow_distance.Get());
    const auto cascade_split_lambda = static_cast<float>(cvar_shadow_cascade_split_lambda.Get());

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
    for(uint32_t i = 0; i < num_cascades; i++) {
        float p = (static_cast<int32_t>(i) + 1) / static_cast<float>(num_cascades);
        float log = z_near * std::pow(ratio, p);
        float uniform = z_near + max_shadow_distance * p;
        float d = cascade_split_lambda * (log - uniform) + uniform;
        cascade_splits[i] = (d - z_near) / clip_range;
    }

    auto last_split_distance = z_near;
    for(auto i = 0u; i < num_cascades; i++) {
        const auto split_distance = cascade_splits[i];

        auto frustum_corners = std::array{
            glm::vec3{-1.0f, 1.0f, -1.0f},
            glm::vec3{1.0f, 1.0f, -1.0f},
            glm::vec3{1.0f, -1.0f, -1.0f},
            glm::vec3{-1.0f, -1.0f, -1.0f},
            glm::vec3{-1.0f, 1.0f, 1.0f},
            glm::vec3{1.0f, 1.0f, 1.0f},
            glm::vec3{1.0f, -1.0f, 1.0f},
            glm::vec3{-1.0f, -1.0f, 1.0f},
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

        for(auto& corner : frustum_corners) {
            const auto transformed_corner = inverse_camera * glm::vec4{corner, 1.f};
            corner = glm::vec3{transformed_corner} / transformed_corner.w;
        }

        // Get frustum center
        auto frustum_center = glm::vec3{0};
        for(const auto& corner : frustum_corners) {
            frustum_center += corner;
        }
        frustum_center /= frustum_corners.size();

        // Fit a sphere to the frustum
        float radius = 0.f;
        for(const auto& corner : frustum_corners) {
            const auto distance = glm::length(corner - frustum_center);
            radius = glm::max(radius, distance);
        }

        radius *= 2;

        // Snap to 16 to avoid texel swimming
        radius = std::ceil(radius * 16.f) / 16.f;

        // Shadow cascade frustum
        const auto light_dir = glm::normalize(glm::vec3{constants.direction_and_size});

        const auto light_view_matrix = glm::lookAt(
            frustum_center - light_dir * radius,
            frustum_center,
            glm::vec3{0.f, 1.f, 0.f}
        );
        const auto light_projection_matrix = glm::ortho(-radius, radius, -radius, radius, 0.f, radius + radius);

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

void DirectionalLight::set_direction(const glm::vec3& direction) {
    constants.direction_and_size = glm::vec4{glm::normalize(direction), 1.f};
    sun_buffer_dirty = true;
}

void DirectionalLight::set_color(const glm::vec4 color) {
    constants.color = color;
    sun_buffer_dirty = true;
}

void DirectionalLight::update_buffer(CommandBuffer& commands) {
    // Write the data to the buffer
    // This is NOT safe. We'll probably write data while the GPU is reading data
    // A better solution might use virtual resources in the frontend and assign real resources
    // just-in-time. That'd solve sync without making the frontend care about frames. We could also
    // just have the frontend care about frames...

    if(constants.shadow_mode != static_cast<uint32_t>(cvar_sun_shadow_mode.Get())) {
        constants.shadow_mode = static_cast<uint32_t>(cvar_sun_shadow_mode.Get());
        sun_buffer_dirty = true;
    }

    if(sun_buffer_dirty) {
        commands.update_buffer(sun_buffer, constants);

        sun_buffer_dirty = false;
    }
}

BufferHandle DirectionalLight::get_constant_buffer() const {
    return sun_buffer;
}

GraphicsPipelineHandle& DirectionalLight::get_pipeline() {
    return pipeline;
}

glm::vec3 DirectionalLight::get_direction() const {
    return glm::normalize(glm::vec3{constants.direction_and_size});
}

void DirectionalLight::render_shadows(RenderGraph& graph, const SceneDrawer& sun_shadow_drawer) const {
    auto& backend = RenderBackend::get();
    if(cvar_sun_shadow_mode.Get() == SunShadowMode::CSM) {
        const auto set = backend.get_transient_descriptor_allocator()
                                .build_set(
                                    {
                                        .bindings = {
                                            {
                                                {
                                                    .binding = 0,
                                                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                    .descriptorCount = 1,
                                                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
                                                }
                                            }
                                        }
                                    })
                                .bind(0, sun_buffer)
                                .build();

        graph.add_render_pass(
            {
                .name = "Sun shadow",
                .descriptor_sets = {set},
                .depth_attachment = RenderingAttachmentInfo{
                    .image = shadowmap_handle,
                    .load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .clear_value = {.depthStencil = {.depth = 1.f}}
                },
                .execute = [&](CommandBuffer& commands) {
                    commands.bind_descriptor_set(0, set);

                    sun_shadow_drawer.draw(commands);

                    commands.clear_descriptor_set(0);
                }
            });
    }
}

void DirectionalLight::render(
    CommandBuffer& commands, const DescriptorSet& gbuffers_descriptor_set, const SceneTransform& view,
    const AccelerationStructureHandle rtas
) const {
    ZoneScoped;

    commands.begin_label(__func__);

    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();

    // Hardware PCF sampler
    auto sampler = allocator.get_sampler(
        VkSamplerCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .compareEnable = VK_TRUE,
            .compareOp = VK_COMPARE_OP_LESS,
            .minLod = 0,
            .maxLod = 16,
        }
    );

    commands.bind_pipeline(pipeline);

    commands.bind_descriptor_set(0, gbuffers_descriptor_set);

    const auto sun_descriptor_set = backend.get_transient_descriptor_allocator()
                                           .build_set(pipeline, 1)
                                           .bind(0, shadowmap_handle, sampler)
                                           .bind(1, sun_buffer)
                                           .bind(2, view.get_buffer())
                                           .bind(3, rtas)
                                           .build();

    commands.bind_descriptor_set(1, sun_descriptor_set);

    commands.draw_triangle();

    commands.clear_descriptor_set(0);
    commands.clear_descriptor_set(1);

    commands.end_label();
}

TextureHandle DirectionalLight::get_shadowmap_handle() const {
    return shadowmap_handle;
}
