#include "halton_sequence.hpp"

#include <cfloat>
#include <cmath>

HaltonSequence::HaltonSequence(const float base_in) : base{base_in} {}

float HaltonSequence::get_next_value() {
    auto x = d - n;
    if(abs(x - 1.f) < FLT_EPSILON) {
        n = 1;
        d *= base;
    } else {
        auto y = d / base;

        while(x <= y) {
            y /= base;
        }
        n = (base + 1) * y - x;
    }
    return n / d;
}
