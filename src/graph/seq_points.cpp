#include "graph/seq_points.hpp"

#include <cmath>

namespace graph {

SeqSource::SeqSource(int seq_index, double plot_start, double n_max, double plot_step)
    : s_(seq_index),
      n0_(std::lround(plot_start)),
      n_max_(std::lround(n_max)),
      step_(plot_step < 1 ? 1 : std::lround(plot_step)) {}

void SeqSource::begin(const Viewport& /*vp*/) {
    n_ = n0_;
}

bool SeqSource::next(double* x_data, double* y_data, bool* defined) {
    if (n_ > n_max_) {
        return false;
    }
    const double y = math::seqexpr::value(s_, n_);
    *x_data = static_cast<double>(n_);
    *y_data = y;
    *defined = std::isfinite(y);
    n_ += step_;
    return true;
}

math::seqexpr::SeqDef make_seq_def(const GraphState& st) {
    math::seqexpr::SeqDef def;
    for (int s = 0; s < kSeqSlots; ++s) {
        def.expr[s] = st.seq.expr[s];
        def.seed1[s] = st.seq.seed1[s];
        def.seed2[s] = st.seq.seed2[s];
    }
    def.n_min = std::lround(st.n_min);
    return def;
}

}  // namespace graph
