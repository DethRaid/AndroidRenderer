#pragma once

/**
 * \brief A bunch of buttons that can be down or up
 */
enum class InputButtons {
    FlycamEnabled
};

enum class InputAction {
    Pressed,
    Released,
};

struct InputEvent {
    InputButtons button;
    InputAction action;
};
