#include "render_graph.hpp"

#include <spdlog/sinks/android_sink.h>
#include <spdlog/logger.h>

#include <volk.h>

#include "render/backend/render_backend.hpp"
#include "core/system_interface.hpp"

static std::shared_ptr<spdlog::logger> logger;

RenderGraph::RenderGraph(RenderBackend& backend_in) : backend{ backend_in }, cmds{ backend.create_command_buffer() } {
    if (logger == nullptr) {
        logger = SystemInterface::get().get_logger("RenderGraph");
    }
    
    cmds.begin();
}

void RenderGraph::add_transition_pass(TransitionPass&& pass) {
    add_compute_pass({.name = "Transition pass", .textures = pass.textures, .buffers = pass.buffers, .execute = [](CommandBuffer&) {}});
}

void RenderGraph::add_compute_pass(ComputePass&& pass) {
    // ZoneScopedN(pass.name.c_str());

    // GpuZoneScopedN(cmds, pass.name.c_str());

    cmds.begin_label(pass.name);

    for(const auto& buffer_token : pass.buffers) {
        cmds.set_resource_usage(buffer_token.first, buffer_token.second.stage, buffer_token.second.access);
    }

    for(const auto& texture_token : pass.textures) {
        cmds.set_resource_usage(texture_token.first, texture_token.second.stage, texture_token.second.access, texture_token.second.layout);
    }

    pass.execute(cmds);

    cmds.end_label();
}

void RenderGraph::add_render_pass(RenderPass&& pass) {
    // ZoneScopedN(pass.name.c_str());

    // GpuZoneScopedN(cmds, pass.name.c_str());

    cmds.begin_label(pass.name);

    for (const auto& buffer_token : pass.buffers) {
        cmds.set_resource_usage(buffer_token.first, buffer_token.second.stage, buffer_token.second.access);
    }

    for (const auto& texture_token : pass.textures) {
        cmds.set_resource_usage(texture_token.first, texture_token.second.stage, texture_token.second.access, texture_token.second.layout);
    }

    auto framebuffer = Framebuffer::create(backend, pass.render_targets, pass.depth_target, pass.render_pass);

    cmds.begin_render_pass(pass.render_pass, framebuffer, pass.clear_values);

    auto first_subpass = true;

    for(const auto& subpass : pass.subpasses) {
        if(!first_subpass) {
            cmds.advance_subpass();
        }

        // ZoneScopedN(subpass.name.c_str());

        // GpuZoneScopedN(cmds, subpass.name.c_str());

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
