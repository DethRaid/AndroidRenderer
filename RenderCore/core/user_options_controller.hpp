#pragma once

#include <functional>
#include <stdexcept>
#include <string>
#include <EASTL/vector.h>
#include <EASTL/unordered_map.h>

#include "console/cvars.hpp"

class CvarNotFoundException final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class NotImplementedException final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * \brief Controller for the user options
 *
 * Receives input from the eternal world in the form of cvar changes. Dispatches those changes to relevant listeners
 *
 * The cvars are the view. The rest of the application is the model. This is the controller. It receives input from the cvars and executes the functions that control the model
 */
class CvarChangeDispatcher {
public:
    template<typename CvarType>
    void register_cvar_listener(std::string_view cvar_name, std::function<void(CvarType)> listener);

    template<typename CvarType>
    void on_cvar_changed(uint32_t name_hash, const CvarType& value);

private:
    template <typename CvarType>
    using CvarListenerMap = eastl::unordered_map<uint32_t, eastl::vector<std::function<void(CvarType value)>>>;

    CvarListenerMap<int32_t> int_cvar_listeners;
    CvarListenerMap<double> float_cvar_listeners;
    CvarListenerMap<std::string> string_cvar_listeners;
};
