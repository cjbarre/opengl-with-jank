#pragma once

#include <math.h>

namespace strafe {

inline float normalize_angle(float a) {
    const float PI = 3.14159265358979f;
    const float TWO_PI = 6.28318530717959f;
    while (a > PI) a -= TWO_PI;
    while (a < -PI) a += TWO_PI;
    return a;
}

} // namespace strafe
