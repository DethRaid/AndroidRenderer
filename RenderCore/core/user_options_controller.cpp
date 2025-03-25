#include "user_options_controller.hpp"

#include <spdlog/fmt/bundled/format.h>

template <>
void CvarChangeDispatcher::register_cvar_listener<int32_t>(
    std::string_view cvar_name, std::function<void(int32_t)> listener
) {
    auto* cvar_system = CVarSystem::Get();
    const auto hash = StringUtils::StringHash{cvar_name}.computedHash;
    const auto* cvar = static_cast<void*>(cvar_system->GetIntCVar(hash));
    if (cvar != nullptr) {
        auto itr = int_cvar_listeners.find(hash);
        if (itr == int_cvar_listeners.end()) {
            itr = int_cvar_listeners.emplace(hash, eastl::vector<std::function<void(int32_t)>>{}).first;
        }

        itr->second.emplace_back(std::move(listener));
        return;
    }

    throw CvarNotFoundException{ fmt::format("No such cvar: {}", std::string_view{cvar_name.data(), cvar_name.size()}) };
}

template <>
void CvarChangeDispatcher::register_cvar_listener<double>(
    std::string_view cvar_name, std::function<void(double)> listener
) {
    auto* cvar_system = CVarSystem::Get();
    const auto hash = StringUtils::StringHash{cvar_name}.computedHash;
    const auto* cvar = static_cast<void*>(cvar_system->GetFloatCVar(hash));
    if (cvar != nullptr) {
        auto itr = float_cvar_listeners.find(hash);
        if (itr == float_cvar_listeners.end()) {
            itr = float_cvar_listeners.emplace(hash, eastl::vector<std::function<void(double)>>{}).first;
        }

        itr->second.emplace_back(std::move(listener));
        return;
    }

    throw CvarNotFoundException{fmt::format("No such cvar: {}", std::string_view{cvar_name.data(), cvar_name.size()})};
}

template <>
void CvarChangeDispatcher::register_cvar_listener<std::string>(
    std::string_view cvar_name, std::function<void(std::string)> listener
) {
    auto* cvar_system = CVarSystem::Get();
    const auto hash = StringUtils::StringHash{cvar_name}.computedHash;
    const auto* cvar = static_cast<const void*>(cvar_system->GetStringCVar(hash));
    if (cvar != nullptr) {
        auto itr = string_cvar_listeners.find(hash);
        if (itr == string_cvar_listeners.end()) {
            itr = string_cvar_listeners.emplace(hash, eastl::vector<std::function<void(std::string)>>{}).first;
        }

        itr->second.emplace_back(std::move(listener));
        return;
    }

    throw CvarNotFoundException{fmt::format("No such cvar: {}", std::string_view{cvar_name.data(), cvar_name.size()})};
}

template<>
void CvarChangeDispatcher::on_cvar_changed<int32_t>(const uint32_t name_hash, const int32_t& value) {
    const auto itr = int_cvar_listeners.find(name_hash);
    if (itr != int_cvar_listeners.end()) {
        for (const auto& listener : itr->second) {
            listener(value);
        }
    }
}

template<>
void CvarChangeDispatcher::on_cvar_changed<double>(const uint32_t name_hash, const double& value) {
    const auto itr = float_cvar_listeners.find(name_hash);
    if (itr != float_cvar_listeners.end()) {
        for (const auto& listener : itr->second) {
            listener(value);
        }
    }
}

template<>
void CvarChangeDispatcher::on_cvar_changed<std::string>(const uint32_t name_hash, const std::string& value) {
    const auto itr = string_cvar_listeners.find(name_hash);
    if (itr != string_cvar_listeners.end()) {
        for (const auto& listener : itr->second) {
            listener(value);
        }
    }
}
