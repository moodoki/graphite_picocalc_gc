#include "math/scratch.hpp"

namespace math::scratch {

namespace {
// The one backing allocation. 16-byte aligned so owners may overlay a
// Complex (two doubles) view. Zero-init (bss) like the private buffers it
// replaces.
alignas(16) std::uint8_t g_arena[kArenaBytes];
}  // namespace

std::uint8_t* compute_region() {
    return g_arena;
}
std::uint8_t* listops_region() {
    return g_arena + kComputeBytes;
}

}  // namespace math::scratch
