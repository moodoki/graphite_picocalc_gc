#include "platform/psram.hpp"

extern "C" {
#include "rp2040-psram/psram_spi.h"
}

namespace platform {

namespace {
psram_spi_inst_t g_psram;
}

bool Psram::init() {
    g_psram = psram_spi_init(pio1, -1);
    // Cheap read-back self-test at two addresses.
    psram_write32(&g_psram, 0, 0xDEADBEEF);
    psram_write32(&g_psram, 4 * 1024 * 1024, 0x12345678);
    ok_ = psram_read32(&g_psram, 0) == 0xDEADBEEF &&
          psram_read32(&g_psram, 4 * 1024 * 1024) == 0x12345678;
    next_ = 0;
    return ok_;
}

uint32_t Psram::alloc(size_t bytes, size_t alignment) {
    if (!ok_ || alignment == 0) {
        return kInvalid;
    }
    const uint32_t aligned = (next_ + (alignment - 1)) & ~static_cast<uint32_t>(alignment - 1);
    if (aligned + bytes > total_bytes()) {
        return kInvalid;
    }
    next_ = aligned + bytes;
    return aligned;
}

void Psram::reset() {
    next_ = 0;
}

void Psram::write(uint32_t addr, const uint8_t* data, size_t len) {
    psram_write(&g_psram, addr, data, len);
}

void Psram::read(uint32_t addr, uint8_t* data, size_t len) {
    psram_read(&g_psram, addr, data, len);
}

Psram& psram() {
    static Psram instance;
    return instance;
}

}  // namespace platform
