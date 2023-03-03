#include "render_graph.hpp"

#include <spdlog/sinks/android_sink.h>
#include <spdlog/logger.h>

#include <volk.h>

#include "utils.hpp"
#include "render/backend/render_backend.hpp"
#include "core/system_interface.hpp"

static std::shared_ptr<spdlog::logger> logger;

RenderGraph::RenderGraph(RenderBackend& backend_in) : backend{backend_in}, cmds{backend.create_command_buffer()} {
    if (logger == nullptr) {
        logger = SystemInterface::get().get_logger("RenderGraph");
    }

    cmds.begin();
}

void RenderGraph::add_transition_pass(TransitionPass&& pass) {
    add_compute_pass(
        {
            .name = "Transition pass", .textures = pass.textures, .buffers = pass.buffers,
            .execute = [](CommandBuffer&) {}
        }
    );
}

void RenderGraph::add_compute_pass(ComputePass&& pass) {
    // ZoneScopedN(pass.name);

    // GpuZoneScopedN(cmds, pass.name);

    cmds.begin_label(pass.name);

    for (const auto& buffer_token : pass.buffers) {
        cmds.set_resource_usage(buffer_token.first, buffer_token.second.stage, buffer_token.second.access);
    }

    for (const auto& texture_token : pass.textures) {
        cmds.set_resource_usage(
            texture_token.first, texture_token.second.stage, texture_token.second.access, texture_token.second.layout
        );
    }

    pass.execute(cmds);

    cmds.end_label();
}

void RenderGraph::add_render_pass(RenderPass&& pass) {
    auto& allocator = backend.get_global_allocator();
    const auto render_pass = allocator.get_render_pass(pass);

    cmds.begin_label(pass.name);

    for (const auto& buffer_token : pass.buffers) {
        cmds.set_resource_usage(buffer_token.first, buffer_token.second.stage, buffer_token.second.access);
    }

    for (const auto& texture_token : pass.textures) {
        cmds.set_resource_usage(
            texture_token.first, texture_token.second.stage, texture_token.second.access, texture_token.second.layout
        );
    }
    
    auto render_targets = std::vector<TextureHandle>{};
    render_targets.reserve(pass.render_targets.size());
    auto depth_target = tl::optional<TextureHandle>{};
    for (const auto& render_target : pass.render_targets) {
        const auto& render_target_actual = allocator.get_texture(render_target);
        if (is_depth_format(render_target_actual.create_info.format)) {
            depth_target = render_target;
            cmds.set_resource_usage(
                render_target, 
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            );
        } else {
            render_targets.push_back(render_target);
            cmds.set_resource_usage(
                render_target, 
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            );
        }
    }

    auto framebuffer = Framebuffer::create(backend, render_targets, depth_target, render_pass);

    cmds.begin_render_pass(render_pass, framebuffer, pass.clear_values);

    auto first_subpass = true;

    for (const auto& subpass : pass.subpasses) {
        if (!first_subpass) {
            cmds.advance_subpass();
        }

        cmds.begin_label(subpass.name);

        subpass.execute(cmds);

        cmds.end_label();

        first_subpass = false;
    }

    cmds.end_render_pass();

    cmds.end_label();

    backend.get_global_allocator().destroy_framebuffer(std::move(framebuffer));
}

void RenderGraph::finish() {
    cmds.end();

    backend.submit_command_buffer(std::move(cmds));
}
