#include "graph/trace.hpp"

#include <algorithm>

namespace graph {

void TraceCursor::step(int dir, int max_index) {
    index += dir;
    clamp(max_index);
}

void TraceCursor::clamp(int max_index) {
    index = std::max(0, std::min(index, max_index));
}

}  // namespace graph
