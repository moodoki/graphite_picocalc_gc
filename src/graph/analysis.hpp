#pragma once

#include <cstdint>

#include "graph/graph_state.hpp"

// Graph-analysis engine (Phase 4B, phase4-spec §4.1/4.2): the numeric
// CALC operations over the live GraphState, mode-aware (function,
// parametric, polar). Pure math layer — the interactive flow lives in
// analysis_cursor, the drawing in apps/graph_screen. Host-testable.
namespace graph {

enum class AnalysisOp : uint8_t {
    kValue,
    kZero,
    kMinimum,
    kMaximum,
    kIntersect,
    kDerivative,
    kIntegral,
};
constexpr int kAnalysisOpCount = 7;

struct AnalysisResult {
    bool ok = false;
    double indep = 0;  // Independent value at the result: x, t, or theta
    double x = 0;      // Plot-space point of the result
    double y = 0;
    double aux = 0;               // Slope (dy/dx), integral value, or r (polar)
    const char* error = nullptr;  // Static string when !ok
};

// value: evaluate slot at an independent value. Parametric reports
// (X(t), Y(t)); polar reports r in aux plus the Cartesian point.
AnalysisResult analyze_value(const GraphState& st, int slot, double indep);

// zero: root of Y (function), Y_T(t) (parametric), or r(theta) (polar)
// in [lo, hi]; falls back to Newton from `guess` when the bracket has
// no sign change. The root must land inside the bounds (TI behavior).
AnalysisResult analyze_zero(const GraphState& st, int slot, double lo, double hi, double guess);

// minimum/maximum of Y, Y_T, or r in [lo, hi] (Brent).
AnalysisResult analyze_extremum(const GraphState& st, int slot, double lo, double hi,
                                bool find_max);

// intersect: same-independent-variable crossing of two slots — solves
// Ya(x) = Yb(x) (or Ya_T(t) = Yb_T(t), ra(th) = rb(th)) near `guess`.
// Parametric/polar note: this finds equal points at equal parameter
// values (r_a(th) = r_b(th) for polar — the classroom method), not
// crossings the curves reach at different parameters.
AnalysisResult analyze_intersect(const GraphState& st, int slot_a, int slot_b, double guess);

// dy/dx at a point: central difference + Richardson, step scaled to
// the window/parameter range (spec §4.2). Parametric slope is
// (dy/dt)/(dx/dt); polar differentiates the Cartesian forms
// r*cos/sin(theta), which equals the classical polar formula and
// stays correct in degree mode. Slope lands in aux.
AnalysisResult analyze_derivative(const GraphState& st, int slot, double at);

// fnInt over [a, b]: adaptive Gauss-Kronrod. Function mode integrates
// Y dx; parametric integrates Y(t) X'(t) dt; polar computes the area
// (1/2) integral of r^2 d(theta) — in radians regardless of angle
// mode, so the value is a true area. Value lands in aux.
AnalysisResult analyze_integral(const GraphState& st, int slot, double a, double b);

}  // namespace graph
