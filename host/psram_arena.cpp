// math::psram_backend over a malloc'd bump arena (Phase 6.4.0).
//
// Lifted verbatim from tests/host/host_psram_backend.cpp, which has been
// doing exactly this job for the host test suite since Phase 3. That it
// transplanted unchanged is the point: the Array/lists/matrix stack talks
// to a four-function seam (math/array.hpp) and never to PSRAM, so the
// whole data layer runs here for free.
//
// Spec section 5.4: this cannot reproduce issue #24's intermittent PSRAM
// read fault, by construction. There is no PSRAM. Do not use the host
// build to investigate it.

#include <cstdlib>
#include <cstring>

#include "math/array.hpp"

namespace math::psram_backend {

namespace {

constexpr size_t kArenaBytes = 4u * 1024 * 1024;
uint8_t* g_arena = nullptr;
size_t g_next = 0;

uint8_t* arena() {
    if (g_arena == nullptr) {
        g_arena = static_cast<uint8_t*>(std::malloc(kArenaBytes));
    }
    return g_arena;
}

}  // namespace

bool available() {
    return true;
}

uint32_t alloc(size_t bytes) {
    if (g_next + bytes > kArenaBytes) {
        return kInvalid;
    }
    const uint32_t addr = static_cast<uint32_t>(g_next);
    g_next += (bytes + 7u) & ~static_cast<size_t>(7u);
    return addr;
}

void read(uint32_t addr, void* out, size_t len) {
    std::memcpy(out, arena() + addr, len);
}

void write(uint32_t addr, const void* src, size_t len) {
    std::memcpy(arena() + addr, src, len);
}

}  // namespace math::psram_backend
