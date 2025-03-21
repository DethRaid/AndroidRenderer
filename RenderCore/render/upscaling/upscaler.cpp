#include "upscaler.hpp"

glm::vec2 IUpscaler::get_jitter() {
    return glm::vec2{jitter_sequence_x.get_next_value(), jitter_sequence_y.get_next_value()};
}
