#pragma once

namespace math {

// Calculator numeric type. double everywhere: ROM softfloat on Pico 1
// (slow but correct), and correct display rounding on both boards.
// Graph-eval float fallback is decision D5 (deferred to profiling).
using calc_t = double;

enum class AngleMode { kRadians, kDegrees };

AngleMode angle_mode();
void set_angle_mode(AngleMode m);

}  // namespace math
