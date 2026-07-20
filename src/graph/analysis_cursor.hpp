#pragma once

#include "graph/analysis.hpp"

// Interactive CALC-operation session (Phase 4B, phase4-spec §4.3).
// Passive state machine in the TraceCursor mold: GraphScreen owns one,
// routes keys (arrows ride the cursor, ENTER commits, ESC cancels),
// and draws from it; this layer only tracks the step flow and runs the
// final computation. Host-testable (no platform dependencies).
namespace graph {

// Menu/readout label for an operation ("Value", "Zero", ...).
const char* analysis_op_name(AnalysisOp op);

struct AnalysisSession {
    bool active = false;  // Collecting inputs
    bool done = false;    // result is valid; readout showing
    AnalysisOp op = AnalysisOp::kValue;
    int slot = 0;         // Curve the cursor rides (first curve for intersect)
    int slot2 = -1;       // Second curve (intersect only)
    int step = 0;         // Which input is being collected
    double vals[3] = {};  // Committed independent values
    AnalysisResult result;

    void begin(AnalysisOp o, int first_slot);
    void cancel();

    int steps_needed() const;

    // Does the current step pick a curve (intersect's first two)?
    bool curve_pick() const;

    // Once bounds are being collected the cursor must stay on the
    // chosen curve — Up/Down slot cycling is disabled (TI behavior).
    bool slot_locked() const;

    // Prompt for the current step ("Left Bound?", "Guess?", "X?", ...).
    const char* prompt(Mode mode) const;

    // Commit the current step: cursor_slot = curve the cursor rides,
    // indep = its independent value. Returns true when all inputs are
    // collected — the caller then calls compute(). Picking the same
    // curve twice for intersect is refused (no advance).
    bool commit(int cursor_slot, double indep);

    // Run the operation; fills result, sets done (input phase ends).
    void compute(const GraphState& st);
};

}  // namespace graph
