#pragma once

class HaltonSequence {
public:
    explicit HaltonSequence(float base_in);

    float get_next_value();

private:
    float base;
    float n = 0;
    float d = 1;
};
