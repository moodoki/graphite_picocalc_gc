#pragma once

#include "config.hpp"
#include "math/format.hpp"
#include "math/types.hpp"
#include "graph/graph_mode.hpp"
#include "graph/table_config.hpp"

namespace platform {
class Storage;
}

namespace graph {

constexpr int kFunctionSlots = 7;    // Y1..Y7
constexpr int kParametricSlots = 6;  // (X1T,Y1T)..(X6T,Y6T)
constexpr int kPolarSlots = 6;       // r1..r6

// Shared x/y canvas window. All modes plot into it; parametric/polar
// add their parameter ranges in GraphState. Defaults are ZStandard.
struct GraphWindow {
    double x_min = -10.0;
    double x_max = 10.0;
    double y_min = -10.0;
    double y_max = 10.0;
    double x_scl = 1.0;
    double y_scl = 1.0;
};

struct YFunctions {
    char expr[kFunctionSlots][config::kMaxExprLen] = {};
    bool enabled[kFunctionSlots] = {};

    bool any_enabled() const {
        for (int i = 0; i < kFunctionSlots; ++i) {
            if (enabled[i] && expr[i][0] != 0) {
                return true;
            }
        }
        return false;
    }
};

struct ParametricFunctions {
    char x_expr[kParametricSlots][config::kMaxExprLen] = {};
    char y_expr[kParametricSlots][config::kMaxExprLen] = {};
    // A pair plots only if enabled AND both exprs are non-empty (§5.1).
    bool enabled[kParametricSlots] = {};
};

struct PolarFunctions {
    char expr[kPolarSlots][config::kMaxExprLen] = {};
    bool enabled[kPolarSlots] = {};
};

// All graphing state across modes (spec §9). Nested rather than flat so
// Phase 1 screens keep working against GraphWindow/YFunctions references;
// content matches the spec's field list.
struct GraphState {
    Mode mode = Mode::kFunction;

    YFunctions y;  // function mode (Phase 1)
    ParametricFunctions param;
    PolarFunctions polar;

    GraphWindow window;

    // Parametric parameter range (§5.2). Tstep default ~= 2*pi/63,
    // TI's ~63 steps over a 2*pi range.
    double t_min = 0.0;
    double t_max = 6.28318530717958647692;
    double t_step = 6.28318530717958647692 / 63.0;

    // Polar angle range (§6.2).
    double theta_min = 0.0;
    double theta_max = 6.28318530717958647692;
    double theta_step = 0.05;

    TableConfig table;

    // MODE-row math settings (persisted since PCG2 — DEG/RAD reset on
    // every boot before that, HW 2026-07-18). The live values stay in
    // math::*; screens changing them must mirror here and save, and
    // load_graph_state() applies them back.
    math::AngleMode angle = math::AngleMode::kRadians;
    math::DisplayMode display = math::DisplayMode::kFloat;
    int fix_digits = 2;

    // Unified persistence (task 2.23): magic-tagged binary image at
    // /picocalc/graphstate.dat, superseding Phase 1's yfuncs.txt and
    // window.dat. A layout change bumps the magic; stale images load
    // as false and the caller keeps defaults (or migrates legacy files).
    // Defined in graph_persist.cpp (kept out of host-test links).
    bool save(platform::Storage& storage) const;
    bool load(platform::Storage& storage);
};

// The single live graph state.
GraphState& state();

}  // namespace graph
