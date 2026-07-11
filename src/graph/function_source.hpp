#pragma once

#include "math/engine.hpp"
#include "graph/plotter.hpp"

namespace graph {

// PointSource for function mode (task 2.3): iterates x across the
// viewport's pixel columns and evaluates one compiled Y-function —
// Phase 1's recompute() inner loop, wrapped.
class FunctionSource : public PointSource {
public:
    // handle = an Engine::compile() result. Not owned; the caller frees
    // it via Engine::free_compiled after plotting.
    FunctionSource(math::Engine& eng, void* handle) : eng_(eng), handle_(handle) {}

    void begin(const Viewport& vp) override;
    bool next(double* x_data, double* y_data, bool* defined) override;

private:
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members) short-lived iterator
    math::Engine& eng_;
    void* handle_;
    const Viewport* vp_ = nullptr;
    int col_ = 0;
};

}  // namespace graph
