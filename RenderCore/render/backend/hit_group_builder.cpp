#include "hit_group_builder.hpp"

#include "render/backend/pipeline_cache.hpp"
#include "core/system_interface.hpp"

HitGroupBuilder::HitGroupBuilder(PipelineCache& cache_in) : cache{cache_in} {
    groups[0].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[1].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
}

HitGroupBuilder& HitGroupBuilder::set_name(const std::string_view name_in) {
    name = name_in;

    return *this;
}

HitGroupBuilder& HitGroupBuilder::add_occlusion_closesthit_shader(const std::filesystem::path& shader_path) {
    const auto shader_maybe = SystemInterface::get().load_file(shader_path);
    if(!shader_maybe) {
        throw std::runtime_error{fmt::format("Could not load closesthit shader {}", shader_path.string())};
    }

    occlusion_closesthit_shader = *shader_maybe;

    return *this;
}

HitGroupBuilder& HitGroupBuilder::add_occlusion_anyhit_shader(const std::filesystem::path& shader_path) {
    const auto shader_maybe = SystemInterface::get().load_file(shader_path);
    if(!shader_maybe) {
        throw std::runtime_error{fmt::format("Could not load anyhit shader {}", shader_path.string())};
    }

    occlusion_anyhit_shader = *shader_maybe;

    return *this;
}

HitGroupBuilder& HitGroupBuilder::add_gi_closesthit_shader(const std::filesystem::path& shader_path) {
    const auto shader_maybe = SystemInterface::get().load_file(shader_path);
    if(!shader_maybe) {
        throw std::runtime_error{fmt::format("Could not load closesthit shader {}", shader_path.string())};
    }

    gi_closesthit_shader = *shader_maybe;

    return *this;
}

HitGroupBuilder& HitGroupBuilder::add_gi_anyhit_shader(const std::filesystem::path& shader_path) {
    const auto shader_maybe = SystemInterface::get().load_file(shader_path);
    if(!shader_maybe) {
        throw std::runtime_error{fmt::format("Could not load anyhit shader {}", shader_path.string())};
    }

    gi_anyhit_shader = *shader_maybe;

    return *this;
}

HitGroupHandle HitGroupBuilder::build() {
    return cache.add_hit_group(*this);
}
