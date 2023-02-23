#include "render/backend/render_graph.hpp"
#include "rsm_vpl_pass.hpp"

#include "render/scene_renderer.hpp"
#include "render/backend/render_backend.hpp"
#include "render/sun_light.hpp"

#include "shared/vpl.hpp"
#include "console/cvars.hpp"

RsmVplPhase::RsmVplPhase(SceneRenderer& renderer_in) : renderer{renderer_in} {
    auto& backend = renderer.get_backend();

    vpl_pipeline = backend.begin_building_pipeline("RSM VPL extraction")
                          .set_vertex_shader("shaders/common/fullscreen.vert.spv")
                          .set_fragment_shader("shaders/lighting/rsm_generate_vpls.frag.spv")
                          .set_depth_state(
                              DepthStencilState{.enable_depth_test = VK_FALSE, .enable_depth_write = VK_FALSE}
                          )
                          .build();

    auto& allocator = backend.get_global_allocator();

    const auto num_cascades = *CVarSystem::Get()->GetIntCVar("r.Shadow.NumCascades");
    count_buffers.reserve(num_cascades);
    vpl_buffers.reserve(num_cascades);
    for (uint32_t i = 0; i < num_cascades; i++) {
        count_buffers.emplace_back(
            allocator.create_buffer(fmt::format("VPL Count {}", i), sizeof(uint32_t), BufferUsage::StorageBuffer)
        );
        vpl_buffers.emplace_back(
            allocator.create_buffer(
                fmt::format("VPL List {}", i), sizeof(PackedVPL) * 65536,
                BufferUsage::StorageBuffer
            )
        );
    }
}

void RsmVplPhase::set_rsm(const RsmTargets& rsm_in) {
    rsm = rsm_in;
}

void RsmVplPhase::setup_buffers(RenderGraph& render_graph) {
    auto buffers = std::unordered_map<BufferHandle, BufferUsageToken>{};

    for (const auto& count_buffer : count_buffers) {
        render_graph.add_compute_pass(
            {
                .name = "Clear count buffer",
                .buffers = {
                    {
                        count_buffer,
                        {.stage = VK_PIPELINE_STAGE_TRANSFER_BIT, .access = VK_ACCESS_TRANSFER_WRITE_BIT}
                    }
                },
                .execute = [&](CommandBuffer& commands) { commands.fill_buffer(count_buffer, 0); }
            }
        );

        buffers.emplace(
            count_buffer, BufferUsageToken{
                .stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, .access = VK_ACCESS_SHADER_WRITE_BIT
            }
        );
    }

    for (const auto& vpl_buffer : vpl_buffers) {
        buffers.emplace(
            vpl_buffer, BufferUsageToken{
                .stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, .access = VK_ACCESS_SHADER_WRITE_BIT
            }
        );
    }

    render_graph.add_transition_pass({.buffers = buffers});
}

void RsmVplPhase::render(CommandBuffer& commands, const SunLight& light) {
    auto& backend = renderer.get_backend();
    const auto sampler = backend.get_default_sampler();

    auto count_buffer_bindings = std::vector<vkutil::DescriptorBuilder::BufferInfo>{};
    auto vpl_buffer_bindings = std::vector<vkutil::DescriptorBuilder::BufferInfo>{};

    for (const auto& count_buffer : count_buffers) {
        count_buffer_bindings.emplace_back(vkutil::DescriptorBuilder::BufferInfo{.buffer = count_buffer});
    }
    for (const auto& vpl_buffer : vpl_buffers) {
        vpl_buffer_bindings.emplace_back(vkutil::DescriptorBuilder::BufferInfo{.buffer = vpl_buffer});
    }

    const auto set = backend.create_frame_descriptor_builder()
                            .bind_image(
                                0,
                                {
                                    .sampler = sampler, .image = rsm.rsm_flux,
                                    .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                },
                                VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT
                            )
                            .bind_image(
                                1,
                                {
                                    .sampler = sampler, .image = rsm.rsm_normal,
                                    .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                },
                                VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT
                            )
                            .bind_image(
                                2,
                                {
                                    .sampler = sampler, .image = rsm.rsm_depth,
                                    .image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                },
                                VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT
                            )
                            .bind_buffer(
                                3, {.buffer = light.get_constant_buffer()},
                                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                VK_SHADER_STAGE_FRAGMENT_BIT
                            )
                            .bind_buffer_array(
                                4, count_buffer_bindings, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                VK_SHADER_STAGE_FRAGMENT_BIT
                            )
                            .bind_buffer_array(
                                5, vpl_buffer_bindings, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                VK_SHADER_STAGE_FRAGMENT_BIT
                            )
                            .build();

    commands.bind_descriptor_set(0, *set);

    commands.bind_pipeline(vpl_pipeline);

    commands.draw_triangle();

    commands.clear_descriptor_set(0);
}

const std::vector<BufferHandle>& RsmVplPhase::get_vpl_lists() const {
    return vpl_buffers;
}
