#pragma once

#include <functional>
#include <queue>
#include <EASTL/vector.h>
#include <glm/vec2.hpp>

#include <glm/vec3.hpp>

#include "input/input_event.hpp"
#include "core/system_interface.hpp"

/**
 * Manages input
 *
 * The general idea: the platform layers will sent input events to this class, then this class will dispatch them to
 * handlers as needed
 */
class InputManager {
public:
    /**
     * The platform layers call this to send the raw input to the engine
     *
     * This input need not be normalized
     */
    void set_player_movement(const glm::vec3& raw_axis);

    void set_player_rotation(glm::vec2 rotation_in);

    void add_input_event(const InputEvent& event);

    /**
     * The engine calls this to register input callbacks
     */
    void add_player_movement_callback(const std::function<void(const glm::vec3&)>& new_callback);

    void add_player_rotation_callback(const std::function<void(const glm::vec2&)>& new_callback);

    void add_input_event_callback(const std::function<void(const InputEvent&)>& new_callback);

    /**
     * Dispatches the various registered callbacks
     */
    void dispatch_callbacks();

private:
    glm::vec3 player_movement_input = glm::vec3{ 0 };

    glm::vec2 player_rotation_input = {};

    eastl::vector<std::function<void(const glm::vec3&)>> movement_callbacks;

    eastl::vector<std::function<void(const glm::vec2&)>> rotation_callbacks;

    eastl::vector<std::function<void(const InputEvent&)>> event_callbacks;

    std::queue<InputEvent> events;
};

