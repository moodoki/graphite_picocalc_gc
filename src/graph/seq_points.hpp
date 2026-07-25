#pragma once

#include "math/seq_expr.hpp"
#include "graph/graph_state.hpp"
#include "graph/plotter.hpp"

namespace graph {

// PointSource for sequence mode's time-series plot (4D.7): sweeps
// integer n from plot_start to n_max in plot_step increments and emits
// (n, u(n)) via math::seqexpr (the caller runs seqexpr::begin first
// and saves/restores the engine's 'n' variable around the sweep).
class SeqSource : public PointSource {
public:
    SeqSource(int seq_index, double plot_start, double n_max, double plot_step);

    void begin(const Viewport& vp) override;
    bool next(double* x_data, double* y_data, bool* defined) override;

private:
    int s_;
    long n0_;
    long n_max_;
    long step_;
    long n_ = 0;
};

// The live GraphState's sequence definitions in math::seqexpr's terms
// (pointers into the state — valid as long as graph::state() is).
math::seqexpr::SeqDef make_seq_def(const GraphState& st);

}  // namespace graph
