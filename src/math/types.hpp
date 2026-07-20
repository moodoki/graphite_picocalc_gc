#pragma once

namespace math {

// Calculator numeric type. double everywhere: ROM softfloat on Pico 1
// (slow but correct), and correct display rounding on both boards.
// Graph-eval float fallback is decision D5 (deferred to profiling).
using calc_t = double;

enum class AngleMode { kRadians, kDegrees };

AngleMode angle_mode();
void set_angle_mode(AngleMode m);

// Number mode (Phase 4C, phase4-spec.md §5.2): governs whether the
// home-screen scalar evaluator (math::complexexpr) is reachable at all
// and how a non-real result displays. REAL is the default (P4-9,
// TI parity) — sqrt(-1) etc. raise a domain error rather than showing
// a complex value. Graphing/tables/stats never consult this; they stay
// on the always-real fast path (math::Engine::evaluate_at/compile).
enum class NumberMode { kReal, kRectangular, kPolar };

NumberMode number_mode();
void set_number_mode(NumberMode m);

}  // namespace math
