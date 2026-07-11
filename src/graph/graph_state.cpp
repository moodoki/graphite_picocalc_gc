#include "graph/graph_state.hpp"

namespace graph {

GraphState& state() {
    static GraphState instance;
    return instance;
}

}  // namespace graph
