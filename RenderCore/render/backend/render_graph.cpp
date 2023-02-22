#include "render_graph.hpp"

#include <spdlog/sinks/android_sink.h>
#include <spdlog/logger.h>

#include <volk.h>

#include "core/system_interface.hpp"

static std::shared_ptr<spdlog::logger> logger;

RenderGraph::RenderGraph(RenderBackend& backend_in) : backend{backend_in} {
    if (logger == nullptr) {
        logger = SystemInterface::get().get_logger("RenderGraph");
    }
}

void RenderGraph::add_pass(RenderPass&& pass) {
    passes.emplace_back(pass);

}

bool has_input_attachments(const std::unordered_map<TextureHandle, TextureState>& texture_states) {
    const auto itr = std::find_if(texture_states.begin(), texture_states.end(),
                                  [](const auto& thing) { return thing.second == TextureState::InputAttachment; });

    return itr != texture_states.end();
}

void RenderGraph::execute() {
    /*
     * We do many things
     *
     * First, we look through all the render passes and build the Vulkan structures. We take care to handle subpaszses
     * and subpass dependencies
     *
     * If a pass does not use any input attachments, we create a new render pass for it. If it does use attachments, we
     * create a subpass for it
     */

    std::unordered_map<TextureHandle, TextureState> last_texture_state;

    std::unordered_map<BufferHandle, BufferState> last_buffer_state;

    std::vector<VkAttachmentDescription> attachments_accumulator;
    std::vector<VkSubpassDescription> subpass_accumulator;
    std::vector<VkSubpassDependency> dependency_accumulator;

    tl::optional<std::size_t> render_pass_start_compiled_pass = tl::nullopt;

    auto& allocator = backend.get_global_allocator();

    for (const auto& pass: passes) {
        auto current_compiled_pass = CompiledRenderPass{allocator};

        for (const auto& [texture, state]: pass.textures) {
            if (const auto itr = last_texture_state.find(texture); itr != last_texture_state.end()) {
                current_compiled_pass.add_barrier(texture, itr->second, state);
            }
        }

        for(const auto& attachment : pass.attachments) {
            const auto& texture = allocator.get_texture(attachment.texture);

            // if this attachment is already in the list then that's great!

        }

        if (has_input_attachments(pass.textures)) {
            // Create a subpass for this pass

        } else {
            // If we've been accumulating subpasses, create a render vk_render_pass out of them

            auto rp_create_info = VkRenderPassCreateInfo{
                    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                    .attachmentCount = static_cast<uint32_t>(attachments_accumulator.size()),
                    .pAttachments = attachments_accumulator.data(),
                    .subpassCount = static_cast<uint32_t>(subpass_accumulator.size()),
                    .pSubpasses = subpass_accumulator.data(),
                    .dependencyCount = static_cast<uint32_t>(dependency_accumulator.size()),
                    .pDependencies = dependency_accumulator.data(),
            };

            auto vk_render_pass = VkRenderPass{};
            const auto result = vkCreateRenderPass(backend.get_device().device, &rp_create_info, nullptr, &vk_render_pass);
            if (result != VK_SUCCESS) {
                logger->error("Could not create render vk_render_pass! Vulkan error {}", result);
            }

            auto& compiled_pass = compiled_passes[*render_pass_start_compiled_pass];
            compiled_pass.render_pass = vk_render_pass;

            attachments_accumulator.clear();
            subpass_accumulator.clear();
            dependency_accumulator.clear();

            render_pass_start_compiled_pass = compiled_passes.size();
        }
    }

    VkCommandBuffer cmds = VK_NULL_HANDLE;

    bool is_in_render_pass = false;

    for (const auto& compiled_pass: compiled_passes) {
        for (const auto& barrier: compiled_pass.barrier_groups) {
            vkCmdPipelineBarrier(cmds,
                                 barrier.srcStageMask, barrier.dstStageMask,
                                 barrier.dependencyFlags,
                                 static_cast<uint32_t>(barrier.memory_barriers.size()), barrier.memory_barriers.data(),
                                 static_cast<uint32_t>(barrier.buffer_barriers.size()), barrier.buffer_barriers.data(),
                                 static_cast<uint32_t>(barrier.image_barriers.size()), barrier.image_barriers.data());
        }

        if (compiled_pass.render_pass) {
            if (is_in_render_pass) {
                vkCmdEndRenderPass(cmds);
            }

            auto rp_begin = VkRenderPassBeginInfo{
                    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                    .renderPass = *compiled_pass.render_pass,
                    .framebuffer = {},
                    .renderArea = {},
                    .clearValueCount = {},
                    .pClearValues  ={}
            };
            vkCmdBeginRenderPass(cmds, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

            is_in_render_pass = true;

        } else {
            vkCmdNextSubpass(cmds, VK_SUBPASS_CONTENTS_INLINE);
        }
    }

    passes.clear();
}
