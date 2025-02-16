#include "input_manager.hpp"

#include <glm/geometric.hpp>

void InputManager::set_player_movement(const glm::vec3& raw_axis) {
    if (glm::length(raw_axis) > 0) {
        player_movement_input = glm::normalize(raw_axis);
    } else {
        player_movement_input = raw_axis;
    }
}

void InputManager::set_player_rotation(const glm::vec2 rotation_in) {
    player_rotation_input = rotation_in;
}

void InputManager::add_input_event(const InputEvent& event) {
    events.push(event);
}

void InputManager::add_player_movement_callback(const std::function<void(const glm::vec3&)>& new_callback) {
    movement_callbacks.push_back(new_callback);
}

void InputManager::add_player_rotation_callback(const std::function<void(const glm::vec2&)>& new_callback) {
    rotation_callbacks.emplace_back(new_callback);
}

void InputManager::add_input_event_callback(const std::function<void(const InputEvent&)>& new_callback) {
    event_callbacks.emplace_back(new_callback);
}

void InputManager::dispatch_callbacks() {
    while(!events.empty()) {
        const auto& event = events.front();
        for(const auto& callback : event_callbacks) {
            callback(event);
        }

        events.pop();
    }

    for (const auto& callback : movement_callbacks) {
        callback(player_movement_input);
    }

    player_movement_input = glm::vec3{0};

    for (const auto& callback : rotation_callbacks) {
        callback(player_rotation_input);
    }

    player_rotation_input = glm::vec2{0};
}
