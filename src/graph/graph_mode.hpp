#pragma once

#include <cstdint>

namespace graph {

// Graph mode (Phase 2 task 2.2, spec §3). Phase 1 implicitly assumed
// kFunction; parametric/polar (Phase 2) and sequence (4D.6-8)
// generalize the same plotting machinery.
enum class Mode : uint8_t {
    kFunction,    // y = f(x)
    kParametric,  // x = f(t), y = g(t)
    kPolar,       // r = f(theta)
    kSeq,         // u(n) recurrences over integer n (4D)
};

// Describes how a mode maps parameters to plotted points.
struct ModeDescriptor {
    Mode mode;
    // Swept engine variable: 'x', 't' or 'n'. 0 = polar, which sweeps
    // the engine's dedicated theta slot (math::Variables::kTheta), not
    // a letter variable.
    char independent_var;
    int slot_count;           // 7 (function), 6 (parametric pairs, polar), 3 (seq)
    const char* slot_prefix;  // "Y", "" (parametric uses X/Y pairs), "r", "u"
};

const ModeDescriptor& descriptor_for(Mode m);

}  // namespace graph
