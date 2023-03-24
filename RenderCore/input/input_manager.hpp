#pragma once

#include <functional>

#include <glm/vec3.hpp>

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

    /**
     * The engine calls this to register input callbacks
     */
    void add_player_movement_callback(const std::function<void(const glm::vec3&)>& new_callback);

    /**
     * Dispatches the various registered callbacks
     */
    void dispatch_callbacks();

private:
    glm::vec3 player_movement_input = glm::vec3{ 0 };

    std::vector<std::function<void(const glm::vec3&)>> input_callbacks;
};

