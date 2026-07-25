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
constexpr int kSeqSlots = 3;         // u, v, w (4D.6-8)

// Shared x/y canvas window. All modes plot into it; parametric/polar
// add their parameter ranges in GraphState. Defaults are ZStandard:
// x = +-10, y derived from the full-screen plot aspect (320x280 px)
// so the window is square as displayed — +-10 * 280/320 = +-8.75
// (HW feedback 2026-07-18; +-10 both axes squashed circles ~12.5%).
struct GraphWindow {
    double x_min = -10.0;
    double x_max = 10.0;
    double y_min = -8.75;
    double y_max = 8.75;
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

// Sequence mode (4D.6-8, D38/P4-12): u/v/w recurrences with seeds.
// seed2 (the value at nMin+1) is consumed only when the expression
// references an (n-2) lag — the editor's seed field accepts "{a,b}".
struct SeqFunctions {
    char expr[kSeqSlots][config::kMaxExprLen] = {};
    bool enabled[kSeqSlots] = {};
    double seed1[kSeqSlots] = {};  // u(nMin)
    double seed2[kSeqSlots] = {};  // u(nMin+1)
};

// Statistical plots (sub-phase 3D, D27): three TI-style slots drawn in
// the graph viewport alongside the mode's functions. Configured via
// the typed `plot` command; data comes from the l1..l6 lists at draw
// time (summaries cached by graph::recompute_stat_plots).
enum class StatPlotType : uint8_t {
    kScatter,
    kXyLine,
    kHistogram,
    kBoxPlot,     // Modified box plot: outliers past 1.5 IQR as marks
    kNormalProb,  // Ordered data vs normal quantiles (Blom positions)
};
constexpr int kStatPlotSlots = 3;
constexpr int kStatPlotTypeCount = 5;

struct StatPlotConfig {
    StatPlotType type = StatPlotType::kScatter;
    uint8_t x_list = 0;  // Source list (0-5)
    uint8_t y_list = 1;  // Scatter/xy-line only
    uint8_t mark = 0;    // 0 dot, 1 plus, 2 cross
    bool enabled = false;
    double bin_width = 0;  // Histogram; 0 = auto ((max-min)/10)
};

// All graphing state across modes (spec §9). Nested rather than flat so
// Phase 1 screens keep working against GraphWindow/YFunctions references;
// content matches the spec's field list.
struct GraphState {
    Mode mode = Mode::kFunction;

    YFunctions y;  // function mode (Phase 1)
    ParametricFunctions param;
    PolarFunctions polar;
    SeqFunctions seq;

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

    // Sequence n range (4D.8; TI defaults). Doubles so the window
    // screen's shared field editing applies; consumers round to long.
    double n_min = 1.0;
    double n_max = 10.0;
    double plot_start = 1.0;
    double plot_step = 1.0;
    // Sequence plot style (MODE row): 0 = time series (n vs u(n)),
    // 1 = web (map curve + y=x + cobweb path; own-lag-1 sequences only).
    uint8_t seq_style = 0;

    // Reserved for Batch 4 (4D.9-11): per-Y-slot inequality shading
    // (0 none, 1 above, 2 below) + spare bytes, carried in the PCG6
    // bump so Batch 4 doesn't force a second one-time reset (D38).
    uint8_t shade_mode[kFunctionSlots] = {};
    uint8_t reserved_4d[8] = {};

    TableConfig table;

    // MODE-row math settings (persisted since PCG2 — DEG/RAD reset on
    // every boot before that, HW 2026-07-18). The live values stay in
    // math::*; screens changing them must mirror here and save, and
    // load_graph_state() applies them back.
    math::AngleMode angle = math::AngleMode::kRadians;
    math::DisplayMode display = math::DisplayMode::kFloat;
    int fix_digits = 2;

    // Number mode (Phase 4C, D30). Default REAL (P4-9, TI parity);
    // persisted since PCG5.
    math::NumberMode number = math::NumberMode::kReal;

    // Numeric axis tick labels ('L' on the graph screen). Kept after the
    // Session 10 on-device eval; persisted since PCG3.
    bool axis_labels = true;

    // Stat plots (3D, persisted since PCG4).
    StatPlotConfig stat_plots[kStatPlotSlots];

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
