#include "lpv_gv_voxelizer.hpp"

#include <tl/optional.hpp>

#include "render/backend/render_backend.hpp"
#include "render/backend/render_graph.hpp"

LpvGvVoxelizer::LpvGvVoxelizer(RenderBackend& backend_in, const glm::uvec3 voxel_texture_resolution) : backend{
    backend_in
} {
    auto& allocator = backend.get_global_allocator();

    volume_handle = allocator.create_volume_texture(
        "Voxels", VK_FORMAT_R16G16B16A16_SFLOAT, voxel_texture_resolution, 1, TextureUsage::StorageImage
    );

    pipeline = backend.begin_building_pipeline("LPV Voxelizer")
                      .set_vertex_shader("shaders/voxelizer/voxelizer.vert.spv")
                      .set_geometry_shader("shaders/voxelizer/voxelizer.geom.spv")
                      .set_fragment_shader("shaders/voxelizer/voxelizer.frag.spv")
                      .build();
}

LpvGvVoxelizer::~LpvGvVoxelizer() {
    auto& allocator = backend.get_global_allocator();
    allocator.destroy_texture(volume_handle);
}

void LpvGvVoxelizer::set_view(SceneDrawer&& view) {
    drawer = std::move(view);
}

void LpvGvVoxelizer::voxelize_scene(RenderGraph& graph) {
    graph.add_render_pass(
        RenderPass{
            .name = "Voxelize",
            .textures = {
                {
                    volume_handle,
                    {
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                    }
                }
            },
            .render_targets = {volume_handle},
            .clear_values = {VkClearValue{.color = {.float32 = {0, 0, 0, 0}}}},
            .subpasses = {
                Subpass{
                    .name = "Voxelize",
                    .color_attachments = {0},
                    .execute = [&](CommandBuffer& commands) { drawer.draw(commands); }
                }
            }
        }
    );
}

TextureHandle LpvGvVoxelizer::get_texture() const {
    return volume_handle;
}
