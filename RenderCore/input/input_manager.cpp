#include "input_manager.hpp"

#include <glm/geometric.hpp>

void InputManager::set_player_movement(const glm::vec3& raw_axis) {
    if (glm::length(raw_axis) > 0) {
        player_movement_input = glm::normalize(raw_axis);
    } else {
        player_movement_input = raw_axis;
    }
}

void InputManager::add_player_movement_callback(const std::function<void(const glm::vec3&)>& new_callback) {
    input_callbacks.push_back(new_callback);
}

void InputManager::dispatch_callbacks() {
    for(const auto& callback : input_callbacks) {
        callback(player_movement_input);
    }

    player_movement_input = glm::vec3{ 0 };
}
