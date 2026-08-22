#include "platform/io_scratch.hpp"

namespace platform {

namespace {
// The one backing allocation. 16-byte aligned so owners may overlay a
// Complex (two doubles) view. Zero-init (bss) like the private buffers
// it replaces — which also keeps it out of .data, unlike the graph
// Image it absorbs.
alignas(16) std::uint8_t g_io_scratch[kIoScratchBytes];
}  // namespace

std::uint8_t* io_scratch() {
    return g_io_scratch;
}

}  // namespace platform
