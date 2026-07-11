#include "graph/graph_mode.hpp"

namespace graph {

namespace {
constexpr ModeDescriptor kDescriptors[] = {
    {Mode::kFunction, 'x', 7, "Y"},
    {Mode::kParametric, 't', 6, ""},
    {Mode::kPolar, 0, 6, "r"},
};
}  // namespace

const ModeDescriptor& descriptor_for(Mode m) {
    return kDescriptors[static_cast<uint8_t>(m)];
}

}  // namespace graph
