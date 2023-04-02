#include "box.hpp"

bool does_range_overlap(float min0, float max0, float min1, float max1);

bool Box::overlaps(const Box& other) const {
    return does_range_overlap(min.x, max.x, other.min.x, other.max.x) &&
        does_range_overlap(min.y, max.y, other.min.y, other.max.y) &&
        does_range_overlap(min.z, max.z, other.min.z, other.max.z);
}

bool does_range_overlap(const float min0, const float max0, const float min1, const float max1) {
    return min0 < max1 && max0 > min1;
}
