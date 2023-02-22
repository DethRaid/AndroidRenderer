#pragma once

#include <vector>

#include <glm/mat4x4.hpp>

#include "render/backend/handles.hpp"
#include "render/backend/compute_shader.hpp"

class RenderBackend;
class ResourceAllocator;
class CommandBuffer;
class SceneView;

struct CascadeData {
    glm::mat4 world_to_cascade;

    /**
     * Linked list of VPLs
     */
    BufferHandle vpl_list = BufferHandle::None;

    /**
     * Count of the next free element in the VPL list
     */
    BufferHandle vpl_list_count = BufferHandle::None;

    /**
     * Head of the VPL linked list for each cell. May be 0xFFFFFFFF
     */
    BufferHandle vpl_list_head = BufferHandle::None;
};

/**
 * A light propagation volume, a la Crytek
 *
 * https://www.advances.realtimerendering.com/s2009/Light_Propagation_Volumes.pdf
 *
 * This is actually cascaded LPVs, but I couldn't find that paper
 *
 * Each cascade is 2x as large as the previous cacade, but has the same number of cells
 */
class LightPropagationVolume {
public:
    explicit LightPropagationVolume(RenderBackend& backend);

    void init_resources(ResourceAllocator& allocator);

    /**
     * Updates the transform of this LPV to match the scene view
     */
    void update_cascade_transforms(const SceneView& view);

    void update_buffers(CommandBuffer& commands);

    void inject_lights(CommandBuffer& commands, BufferHandle vpl_list_buffer);

    /**
     * Addatively renders the LPV onto the bound framebuffer
     *
     * @param commands The command buffer to render with. Should already have a framebuffer bound
     * @param gbuffers_descriptor The descriptor set that contains the gbuffer attachments as input attachments
     */
    void add_lighting_to_scene(CommandBuffer& commands, VkDescriptorSet gbuffers_descriptor);

private:
    RenderBackend& backend;

    // We have a A and B LPV, to allow for ping-ponging during the propagatio step

    // Each LPV has a separate texture for the red, green, and blue SH coefficients
    // We store coefficients for the first two SH bands. Future work might add another band, at the cost of 2x the
    // memory

    TextureHandle lpv_a_red = TextureHandle::None;
    TextureHandle lpv_a_green = TextureHandle::None;
    TextureHandle lpv_a_blue = TextureHandle::None;

    TextureHandle lpv_b_red = TextureHandle::None;
    TextureHandle lpv_b_green = TextureHandle::None;
    TextureHandle lpv_b_blue = TextureHandle::None;

    TextureHandle geometry_volume_handle = TextureHandle::None;

    ComputeShader vpl_placement_shader;

    ComputeShader vpl_injection_shader;

    std::vector<CascadeData> cascades;
    BufferHandle cascade_data_buffer = BufferHandle::None;

    /**
     * Renders the LPV into the lighting buffer
     */
    PipelineHandle lpv_render_shader;
};



