// Firmware psram_backend: thin adapter onto the platform PSRAM driver
// (chunked bulk path, D10). Host tests link a malloc shim instead —
// keep logic out of here.

#include "platform/psram.hpp"
#include "math/array.hpp"

namespace math::psram_backend {

bool available() {
    return platform::psram().ok();
}

uint32_t alloc(size_t bytes) {
    auto& ps = platform::psram();
    if (!ps.ok()) {
        return kInvalid;
    }
    return ps.alloc(bytes, 8);
}

void read(uint32_t addr, void* out, size_t len) {
    platform::psram().read(addr, static_cast<uint8_t*>(out), len);
}

void write(uint32_t addr, const void* src, size_t len) {
    platform::psram().write(addr, static_cast<const uint8_t*>(src), len);
}

}  // namespace math::psram_backend
