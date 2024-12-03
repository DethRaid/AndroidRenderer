#include "bloomer.hpp"

#include "backend/pipeline_cache.hpp"
#include "console/cvars.hpp"
#include "render/backend/render_backend.hpp"
#include "core/system_interface.hpp"

static auto cvar_num_bloom_mips = AutoCVar_Int{"r.bloom.NumMips", "Number of mipmaps in the bloom chain", 6};

static std::shared_ptr<spdlog::logger> logger;

Bloomer::Bloomer() {
    if (logger == nullptr) {
        logger = SystemInterface::get().get_logger("Bloomer");
        logger->set_level(spdlog::level::info);
    }

    auto& backend = RenderBackend::get();
    auto& pipeline_cache = backend.get_pipeline_cache();

    downsample_shader = pipeline_cache.create_pipeline("shaders/postprocessing/bloom_downsample.comp.spv");
    upsample_shader = pipeline_cache.create_pipeline("shaders/postprocessing/bloom_upsample.comp.spv");

    bilinear_sampler = backend.get_global_allocator().get_sampler(
        {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .maxLod = 16,
        }
    );
}

void Bloomer::fill_bloom_tex(RenderGraph& graph, const TextureHandle scene_color) {
    ZoneScoped;

    if (bloom_tex == nullptr) {
        create_bloom_tex(scene_color);
    }

    auto& backend = RenderBackend::get();
    const auto bloom_0_set = backend.get_transient_descriptor_allocator().build_set(downsample_shader, 0)
                                    .bind(0, scene_color, bilinear_sampler)
                                    .bind(1, bloom_tex)
                                    .build();
    graph.add_compute_dispatch(
        {
            .name = "Bloom 0",
            .descriptor_sets = {bloom_0_set},
            .num_workgroups = {(bloom_tex_resolution.x + 7) / 8, (bloom_tex_resolution.y + 7) / 8, 1},
            .compute_shader = downsample_shader
        }
    );

    graph.add_pass(
        {
            .name = "Bloom",
            .execute = [&](CommandBuffer& commands) {
                const auto& bloom_texture_actual = backend.get_global_allocator().get_texture(bloom_tex);
                auto dispatch_size = bloom_tex_resolution;

                commands.bind_pipeline(downsample_shader);

                // We gonna rock down to electric avenue
                for (auto pass = 0u; pass < cvar_num_bloom_mips.Get() - 1; pass++) {
                    dispatch_size /= glm::uvec2{2};

                    logger->trace("Bloom downsample pass {}", pass);
                    logger->trace(
                        "Transitioning mip {} of bloom tex from layout VK_IMAGE_LAYOUT_GENERAL to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL",
                        pass
                    );
                    logger->trace(
                        "Transitioning mip {} of bloom tex from layout VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_GENERAL",
                        pass + 1
                    );
                    commands.barrier(
                        {}, {}, {
                            VkImageMemoryBarrier2{
                                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                                .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
                                .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                                .image = bloom_texture_actual.image,
                                .subresourceRange = {
                                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                    .baseMipLevel = pass,
                                    .levelCount = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount = 1
                                }
                            },
                            VkImageMemoryBarrier2{
                                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                                .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                                .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                                .image = bloom_texture_actual.image,
                                .subresourceRange = {
                                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                    .baseMipLevel = pass + 1,
                                    .levelCount = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount = 1
                                }
                            }
                        }
                    );

                    auto set = *vkutil::DescriptorBuilder::begin(
                                    backend, backend.get_transient_descriptor_allocator()
                                )
                                .bind_image(
                                    0, {
                                        .sampler = bilinear_sampler, .image = bloom_tex,
                                        .image_layout = VK_IMAGE_LAYOUT_GENERAL,
                                        .mip_level = pass
                                    }, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT
                                )
                                .bind_image(
                                    1, {
                                        .image = bloom_tex, .image_layout = VK_IMAGE_LAYOUT_GENERAL,
                                        .mip_level = pass + 1
                                    },
                                    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT
                                )
                                .build();

                    commands.bind_descriptor_set(0, set);

                    commands.dispatch((dispatch_size.x + 7) / 8, (dispatch_size.y + 7) / 8, 1);
                }

                // And then we take it higher
                // commands.bind_shader(upsample_shader);
                // 
                // for (auto pass = static_cast<uint32_t>(cvar_num_bloom_mips.Get() - 1); pass > 0; pass--) {
                //     dispatch_size *= glm::uvec2{2};
                // 
                //     logger->trace("Bloom upsample pass {}", pass);
                //     logger->trace("Transitioning mip {} of bloom tex from layout VK_IMAGE_LAYOUT_GENERAL to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL", pass);
                //     logger->trace("Transitioning mip {} of bloom tex from layout VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL to VK_IMAGE_LAYOUT_GENERAL", pass - 1);
                //     commands.barrier(
                //         {}, {}, {
                //             VkImageMemoryBarrier2{
                //                 .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                //                 .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                //                 .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                //                 .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                //                 .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
                //                 .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                //                 .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                //                 .image = bloom_texture_actual.image,
                //                 .subresourceRange = {
                //                     .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                //                     .baseMipLevel = pass,
                //                     .levelCount = 1,
                //                     .baseArrayLayer = 0,
                //                     .layerCount = 1
                //                 }
                //             },
                //             VkImageMemoryBarrier2{
                //                 .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                //                 .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                //                 .srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
                //                 .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                //                 .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                //                 .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                //                 .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                //                 .image = bloom_texture_actual.image,
                //                 .subresourceRange = {
                //                     .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                //                     .baseMipLevel = pass - 1,
                //                     .levelCount = 1,
                //                     .baseArrayLayer = 0,
                //                     .layerCount = 1
                //                 }
                //             }
                //         }
                //     );
                // 
                // 
                //     backend.create_frame_descriptor_builder()
                //            .bind_image(
                //                0, {
                //                    .sampler = bilinear_sampler, .image = bloom_tex,
                //                    .image_layout = VK_IMAGE_LAYOUT_GENERAL,
                //                    .mip_level = pass
                //                }, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT
                //            )
                //            .bind_image(
                //                1, {
                //                    .image = bloom_tex, .image_layout = VK_IMAGE_LAYOUT_GENERAL,
                //                    .mip_level = pass - 1
                //                },
                //                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT
                //            )
                //            .build(set);
                // 
                //     commands.bind_descriptor_set(0, set);
                // 
                //     commands.dispatch((dispatch_size.x + 7) / 8, (dispatch_size.y + 7) / 8, 1);
                // }

                commands.clear_descriptor_set(0);

                // Put the top mip in shader read
                commands.barrier(
                    {}, {}, {
                        VkImageMemoryBarrier2{
                            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                            .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                            .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT,
                            .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                            .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
                            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            .image = bloom_texture_actual.image,
                            .subresourceRange = {
                                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                .baseMipLevel = 0,
                                .levelCount = 6,
                                .baseArrayLayer = 0,
                                .layerCount = 1
                            }
                        }
                    }
                );
            }
        }
    );

    graph.set_resource_usage(
        bloom_tex, {
            .stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, .access = VK_ACCESS_2_SHADER_READ_BIT,
            .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        }, true
    );
}

TextureHandle Bloomer::get_bloom_tex() const { return bloom_tex; }

void Bloomer::create_bloom_tex(const TextureHandle scene_color) {
    auto& backend = RenderBackend::get();
    auto& allocator = backend.get_global_allocator();
    const auto& scene_color_actual = allocator.get_texture(scene_color);

    const auto& create_info = scene_color_actual.create_info;

    bloom_tex_resolution = glm::uvec2{create_info.extent.width, create_info.extent.height} / glm::uvec2{2};

    bloom_tex = allocator.create_texture(
        "Bloom texture", create_info.format, bloom_tex_resolution, cvar_num_bloom_mips.Get(), TextureUsage::StorageImage
    );
}
