#include "mip_chain_generator.hpp"

#define A_CPU
#include "backend/pipeline_cache.hpp"
#include "extern/spd/ffx_a.h"
#include "extern/spd/ffx_spd.h"

#include "render/backend/render_backend.hpp"
#include "core/system_interface.hpp"

MipChainGenerator::MipChainGenerator() {
    // TODO: We need a shader templating system. This will let us build the mip chain generation shaders with a custom
    // texture format and reduction filter

    auto& backend = RenderBackend::get();
    auto& pipeline_cache = backend.get_pipeline_cache();

    shaders.emplace(
        VK_FORMAT_R16_SFLOAT,
        pipeline_cache.create_pipeline("shaders/util/mip_chain_generator_R16F.comp.spv")
    );

    shaders.emplace(
        VK_FORMAT_R16G16B16A16_SFLOAT,
        pipeline_cache.create_pipeline("shaders/util/mip_chain_generator_RGBA16F.comp.spv")
    );

    shaders.emplace(
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        pipeline_cache.create_pipeline(
            "shaders/util/mip_chain_generator_B10G11R11F.comp.spv"
        )
    );

    shaders.emplace(
        VK_FORMAT_R32_SFLOAT,
        pipeline_cache.create_pipeline("shaders/util/mip_chain_generator_D32F_min.comp.spv")
    );

    {
        const auto bytes = *SystemInterface::get().load_file("shaders/util/mip_chain_generator_D32F_min.comp.spv");
        shaders.emplace(VK_FORMAT_R32_SFLOAT, *backend.create_compute_shader("Mip Chain Generator D32F", bytes));
    }

    auto& allocator = backend.get_global_allocator();
    counter_buffer = allocator.create_buffer("SPD Counter Buffer", sizeof(uint32_t) * 6, BufferUsage::StorageBuffer);

    sampler = allocator.get_sampler(
        {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .maxLod = VK_LOD_CLAMP_NONE,
        }
    );
}

void MipChainGenerator::fill_mip_chain(
    RenderGraph& graph, const TextureHandle src_texture, const TextureHandle dest_texture
) {
    graph.add_pass(
        {
            .name = "Clear counter",
            .buffers = {
                {
                    counter_buffer,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT
                }
            },
            .execute = [this](const CommandBuffer& commands) {
                commands.fill_buffer(counter_buffer, 0);
            }
        }
    );
    graph.add_pass(
        {
            .name = "Downsample",
            .textures = {
                {
                    src_texture,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                },
                {
                    dest_texture,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                    VK_IMAGE_LAYOUT_GENERAL
                }
            },
            .buffers = {
                {
                    counter_buffer,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT
                }
            },
            .execute = [=, this](CommandBuffer& commands) {
                auto& backend = RenderBackend::get();

                const auto dest_texture_format = dest_texture->create_info.format;
                const auto& shader = shaders.at(dest_texture_format);

                auto uavs = std::vector<VkDescriptorImageInfo>{};
                uavs.reserve(12);
                for(auto mip_level = 0u; mip_level < dest_texture->create_info.mipLevels; mip_level++) {
                    uavs.emplace_back(
                        VkDescriptorImageInfo{
                            .imageView = dest_texture->mip_views[mip_level],
                            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                        }
                    );
                }
                for(auto mip_level = dest_texture->create_info.mipLevels; mip_level < 12; mip_level++) {
                    uavs.emplace_back(
                        VkDescriptorImageInfo{
                            .imageView = dest_texture->mip_views[1],
                            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                        }
                    );
                }

                auto set = *vkutil::DescriptorBuilder::begin(
                                backend,
                                backend.get_transient_descriptor_allocator()
                            )
                            .bind_image(
                                0,
                                {
                                    .sampler = sampler,
                                    .image = src_texture,
                                    .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                },
                                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                VK_SHADER_STAGE_COMPUTE_BIT
                            )
                            .bind_image(
                                1,
                                uavs.data(),
                                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                VK_SHADER_STAGE_COMPUTE_BIT,
                                static_cast<int32_t>(uavs.size())
                            )
                            .build();

                commands.bind_descriptor_set(0, set);

                commands.bind_pipeline(shader);

                varAU2(dispatch_thread_group_count_xy);
                varAU2(work_group_offset);
                varAU2(num_work_groups_and_mips);
                varAU4(rect_info) = initAU4(
                    0,
                    0,
                    src_texture->create_info.extent.width,
                    src_texture->create_info.extent.height
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
