#include "depth_culling_phase.hpp"

#include <glm/common.hpp>
#include <glm/exponential.hpp>

#include "core/system_interface.hpp"
#include "render/mesh_drawer.hpp"
#include "render/mesh_storage.hpp"
#include "render/render_scene.hpp"
#include "render/backend/pipeline_cache.hpp"
#include "render/backend/render_backend.hpp"
#include "render/backend/resource_allocator.hpp"

DepthCullingPhase::DepthCullingPhase(RenderBackend& backend) :
    backend{backend}, allocator{backend.get_global_allocator()}, downsampler{backend},
    texture_descriptor_pool{backend.get_texture_descriptor_pool()} {
    auto& pipeline_cache = backend.get_pipeline_cache();

    visibility_list_to_draw_commands = pipeline_cache.create_pipeline("shaders/util/visibility_list_to_draw_commands.comp.spv");

    hi_z_culling_shader = pipeline_cache.create_pipeline("shaders/culling/hi_z_culling.comp.spv");

    //add a extension struct to enable Min mode
    VkSamplerReductionModeCreateInfoEXT create_info_reduction = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT,
        .reductionMode = VK_SAMPLER_REDUCTION_MODE_MAX,
    };

    max_reduction_sampler = allocator.get_sampler(
        {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = &create_info_reduction,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .maxLod = 16,
        }
    );
}

DepthCullingPhase::~DepthCullingPhase() {
    if (depth_buffer != TextureHandle::None) {
        allocator.destroy_texture(depth_buffer);
        depth_buffer = TextureHandle::None;
    }
    if (hi_z_buffer != TextureHandle::None) {
        allocator.destroy_texture(hi_z_buffer);
        hi_z_buffer = TextureHandle::None;

        texture_descriptor_pool.free_descriptor(hi_z_index);
        hi_z_index = 0;
    }
    if (visible_objects) {
        allocator.destroy_buffer(visible_objects);
        visible_objects = {};
    }
}

void DepthCullingPhase::set_render_resolution(const glm::uvec2& resolution) {
    if (depth_buffer != TextureHandle::None) {
        allocator.destroy_texture(depth_buffer);
        depth_buffer = TextureHandle::None;
    }
    if (hi_z_buffer != TextureHandle::None) {
        allocator.destroy_texture(hi_z_buffer);
        hi_z_buffer = TextureHandle::None;

        texture_descriptor_pool.free_descriptor(hi_z_index);
        hi_z_index = 0;
    }

    depth_buffer = allocator.create_texture(
        "Depth buffer", VK_FORMAT_D32_SFLOAT, resolution, 1, TextureUsage::RenderTarget
    );

    const auto hi_z_resolution = resolution / 2u;
    const auto major_dimension = glm::max(hi_z_resolution.x, hi_z_resolution.y);
    const auto num_mips = glm::round(glm::log2(static_cast<float>(major_dimension)));
    hi_z_buffer = allocator.create_texture(
        "Hi Z Buffer", VK_FORMAT_R32_SFLOAT, hi_z_resolution, static_cast<uint32_t>(num_mips),
        TextureUsage::StorageImage
    );

    hi_z_index = texture_descriptor_pool.create_texture_srv(hi_z_buffer, max_reduction_sampler);
}

void DepthCullingPhase::render(RenderGraph& graph, const SceneDrawer& drawer, const BufferHandle view_data_buffer) {
    ZoneScoped;

    graph.begin_label("Depth/culling pass");

    const auto view_descriptor = *vkutil::DescriptorBuilder::begin(
        backend, backend.get_transient_descriptor_allocator()
    )
                                         .bind_buffer(
                                             0, {.buffer = view_data_buffer}, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                             VK_SHADER_STAGE_VERTEX_BIT
                                         )
                                         .build();

    auto& scene = drawer.get_scene();
    const auto primitive_buffer = scene.get_primitive_buffer();
    const auto num_primitives = scene.get_total_num_primitives();

    if (!visible_objects) {
        visible_objects = allocator.create_buffer(
            "Visible objects list", sizeof(uint32_t) * num_primitives, BufferUsage::StorageBuffer
        );
    }

    {
        // Translate last frame's list of objects to indirect draw commands

        const auto& [draw_commands_buffer, draw_count_buffer, primitive_id_buffer] =
            translate_visibility_list_to_draw_commands(
                graph, visible_objects, primitive_buffer, num_primitives,
                drawer.get_mesh_storage().get_draw_args_buffer()
            );

        // Draw last frame's visible objects

        graph.begin_render_pass(
            {
                .name = "Rasterize last frame's visible objects",
                .buffers = {
                    {
                        draw_commands_buffer,
                        {VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT}
                    },
                    {draw_count_buffer, {VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT}},
                    {primitive_id_buffer, {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT}},
                },
                .attachments = {depth_buffer},
                .clear_values = {{.depthStencil = {.depth = 1.f}}}
            }
        );
        graph.add_subpass(
            {
                .name = "Rasterize last frame's visible objects",
                .depth_attachment = 0,
                .execute = [&](CommandBuffer& commands) {
                    commands.bind_descriptor_set(0, view_descriptor);

                    drawer.draw_indirect(
                        commands, draw_commands_buffer, draw_count_buffer, primitive_id_buffer
                    );
                }
            }
        );
        graph.end_render_pass();
    }

    // Build Hi-Z pyramid

    downsampler.fill_mip_chain(graph, depth_buffer, hi_z_buffer);

    // Cull all objects against the pyramid, keeping track of newly visible objects

    // All the primitives that are visible this frame, whether they're newly visible or not
    const auto this_frame_visible_objects = allocator.create_buffer(
        "This frame visibility mask", sizeof(uint32_t) * num_primitives, BufferUsage::StorageBuffer
    );

    // Just the primitives that are visible this frame
    const auto newly_visible_objects = allocator.create_buffer(
        "New visibility mask", sizeof(uint32_t) * num_primitives, BufferUsage::StorageBuffer
    );

    graph.add_pass(
        {
            .name = "HiZ Culling",
            .textures = {
                {
                    hi_z_buffer,
                    {
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    }
                }
            },
            .buffers = {
                {primitive_buffer, {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT}},
                {visible_objects, {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT}},
                {newly_visible_objects, {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT}},
                {this_frame_visible_objects, {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT}},
            },
            .execute = [&](CommandBuffer& commands) {
                commands.bind_descriptor_set(0, texture_descriptor_pool.get_descriptor_set());

                commands.bind_buffer_reference(0, primitive_buffer);
                commands.bind_buffer_reference(2, visible_objects);
                commands.bind_buffer_reference(4, newly_visible_objects);
                commands.bind_buffer_reference(6, this_frame_visible_objects);

                commands.bind_buffer_reference(8, view_data_buffer);

                commands.set_push_constant(10, num_primitives);
                commands.set_push_constant(11, hi_z_index);

                commands.bind_pipeline(hi_z_culling_shader);

                commands.dispatch((num_primitives + 95) / 96, 1, 1);

                commands.clear_descriptor_set(0);
            }
        }
    );

    // The destruction will happen after this frame is complete, it's fine
    allocator.destroy_buffer(newly_visible_objects);
    allocator.destroy_buffer(visible_objects);

    // Save the list of visible objects so we can use them next frame
    visible_objects = this_frame_visible_objects;

    {
        // Translate newly visible objects to indirect draw commands
        const auto& [draw_commands_buffer, draw_count_buffer, primitive_id_buffer] =
            translate_visibility_list_to_draw_commands(
                graph, newly_visible_objects, primitive_buffer, num_primitives,
                drawer.get_mesh_storage().get_draw_args_buffer()
            );

        // Draw them

        graph.begin_render_pass(
            {
                .name = "Rasterize this frame's visible objects",
                .buffers = {
                    {
                        draw_commands_buffer,
                        {VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT}
                    },
                    {draw_count_buffer, {VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT}},
                    {primitive_id_buffer, {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT}},
                },
                .attachments = {depth_buffer}
            }
        );
        graph.add_subpass(
            {
                .name = "Rasterize",
                .depth_attachment = 0,
                .execute = [&](CommandBuffer& commands) {
                    commands.bind_descriptor_set(0, view_descriptor);

                    drawer.draw_indirect(
                        commands, draw_commands_buffer, draw_count_buffer, primitive_id_buffer
                    );
                }
            }
        );
        graph.end_render_pass();
    }

    graph.end_label();
}

TextureHandle DepthCullingPhase::get_depth_buffer() const { return depth_buffer; }

BufferHandle DepthCullingPhase::get_visible_objects() const { return visible_objects; }

std::tuple<BufferHandle, BufferHandle, BufferHandle> DepthCullingPhase::translate_visibility_list_to_draw_commands(
    RenderGraph& graph, const BufferHandle visibility_list, const BufferHandle primitive_buffer,
    const uint32_t num_primitives, const BufferHandle mesh_draw_args_buffer
) const {
    const auto draw_commands_buffer = allocator.create_buffer(
        "Draw commands", sizeof(VkDrawIndexedIndirectCommand) * num_primitives, BufferUsage::IndirectBuffer
    );
    const auto draw_count_buffer = allocator.create_buffer("Draw count", sizeof(uint32_t), BufferUsage::IndirectBuffer);
    const auto primitive_id_buffer = allocator.create_buffer(
        "Primitive ID", sizeof(uint32_t) * num_primitives, BufferUsage::StorageBuffer
    );

    graph.add_pass(
        {
            .name = "Clear draw count",
            .buffers = {{draw_count_buffer, {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT}}},
            .execute = [&](const CommandBuffer& commands) {
                commands.fill_buffer(draw_count_buffer);
            }
        }
    );

    graph.add_pass(
        ComputePass{
            .name = "Translate visibility list",
            .buffers = {
                {primitive_buffer, {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT}},
                {visibility_list, {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT}},
                {mesh_draw_args_buffer, {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT}},
                {draw_commands_buffer, {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT}},
                {
                    draw_count_buffer,
                    {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT}
                },
                {primitive_id_buffer, {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT}},

            },
            .execute = [&](CommandBuffer& commands) {
                commands.bind_buffer_reference(0, primitive_buffer);
                commands.bind_buffer_reference(2, visibility_list);
                commands.bind_buffer_reference(4, mesh_draw_args_buffer);
                commands.bind_buffer_reference(6, draw_commands_buffer);
                commands.bind_buffer_reference(8, draw_count_buffer);
                commands.bind_buffer_reference(10, primitive_id_buffer);

                commands.set_push_constant(12, num_primitives);

                commands.bind_pipeline(visibility_list_to_draw_commands);

                commands.dispatch((num_primitives + 95) / 96, 1, 1);
            }
        }
    );

    // Destroy the buffers next frame
    allocator.destroy_buffer(draw_commands_buffer);
    allocator.destroy_buffer(draw_count_buffer);
    allocator.destroy_buffer(primitive_id_buffer);

    return std::make_tuple(draw_commands_buffer, draw_count_buffer, primitive_id_buffer);
}
