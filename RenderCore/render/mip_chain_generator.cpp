#include "mip_chain_generator.hpp"

#define A_CPU
#include "extern/spd/ffx_a.h"
#include "extern/spd/ffx_spd.h"

#include "render/backend/render_backend.hpp"
#include "core/system_interface.hpp"

MipChainGenerator::MipChainGenerator(RenderBackend& backend_in) : backend{backend_in} {
    {
        const auto bytes = *SystemInterface::get().load_file("shaders/util/mip_chain_generator.comp.spv");
        shader = *backend.create_compute_shader("Mip Chain Generator", bytes);
    }

    auto& allocator = backend.get_global_allocator();
    counter_buffer = allocator.create_buffer("SPD Counter Buffer", sizeof(uint32_t) * 6, BufferUsage::StorageBuffer);
}

void MipChainGenerator::fill_mip_chain(RenderGraph& graph, TextureHandle texture) {
    graph.add_compute_pass(
        {
            .name = "Clear counter",
            .buffers = {
                {
                    counter_buffer,
                    {
                        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        VK_ACCESS_2_TRANSFER_WRITE_BIT
                    }
                }
            },
            .execute = [this](CommandBuffer& commands) {
                commands.fill_buffer(counter_buffer, 0);
            }
        }
    );
    graph.add_compute_pass(
        {
            .name = "Downsample",
            .textures = {
                {
                    texture,
                    {
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                        VK_IMAGE_LAYOUT_GENERAL
                    }
                }
            },
            .buffers = {
                {
                    counter_buffer,
                    {
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT
                    }
                }
            },
            .execute = [=, this](CommandBuffer& commands) {
                auto& allocator = backend.get_global_allocator();
                const auto& texture_actual = allocator.get_texture(texture);

                auto uavs = std::vector<VkDescriptorImageInfo>{};
                uavs.reserve(texture_actual.create_info.mipLevels - 1);
                for (auto mip_level = 1u; mip_level < texture_actual.create_info.mipLevels; mip_level++) {
                    uavs.emplace_back(
                        VkDescriptorImageInfo{
                            .imageView = texture_actual.mip_views[mip_level],
                            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                        }
                    );
                }

                auto set = VkDescriptorSet{};
                backend.create_frame_descriptor_builder()
                       .bind_image(
                           0, {
                               .sampler = backend.get_default_sampler(), .image = texture,
                               .image_layout = VK_IMAGE_LAYOUT_GENERAL
                           }, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT
                       )
                       .bind_image(
                           1, uavs.data(), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, uavs.size()
                       )
                       .build(set);

                commands.bind_descriptor_set(0, set);

                commands.bind_shader(shader);

                varAU2(dispatch_thread_group_count_xy);
                varAU2(work_group_offset);
                varAU2(num_work_groups_and_mips);
                varAU4(rect_info) = initAU4(
                    0, 0, texture_actual.create_info.extent.width, texture_actual.create_info.extent.height
                );
                SpdSetup(dispatch_thread_group_count_xy, work_group_offset, num_work_groups_and_mips, rect_info);

                commands.set_push_constant(0, num_work_groups_and_mips[0]);
                commands.set_push_constant(1, num_work_groups_and_mips[1]);
                commands.set_push_constant(2, work_group_offset[0]);
                commands.set_push_constant(3, work_group_offset[1]);
                commands.bind_buffer_reference(4, counter_buffer);

                // Last item is number of slices - 6 for cube textures. We'll need to handle this Soon :tm:
                commands.dispatch(dispatch_thread_group_count_xy[0], dispatch_thread_group_count_xy[1], 1);

                commands.clear_descriptor_set(0);
            }
        }
    );
}
