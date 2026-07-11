#include "platform/psram.hpp"

extern "C" {
#include "rp2040-psram/psram_spi.h"
}

namespace platform {

namespace {
psram_spi_inst_t g_psram;

// Cheap read-back self-test at two addresses.
bool self_test() {
    psram_write32(&g_psram, 0, 0xDEADBEEF);
    psram_write32(&g_psram, 4 * 1024 * 1024, 0x12345678);
    return psram_read32(&g_psram, 0) == 0xDEADBEEF &&
           psram_read32(&g_psram, 4 * 1024 * 1024) == 0x12345678;
}
}  // namespace

bool Psram::init() {
    g_psram = psram_spi_init(pio1, -1);
    ok_ = self_test();
    next_ = 0;
    return ok_;
}

bool Psram::reinit() {
    if (ok_) {
        return true;
    }
    // Re-send reset-enable/reset through the already-configured PIO.
    // Deliberately NOT psram_spi_init(): every call re-adds the PIO
    // program (32-instruction budget) and claims two DMA channels, so
    // repeated re-init would exhaust both.
    uint8_t reset_en[] = {8, 0, 0x66u};
    pio_spi_write_read_dma_blocking(&g_psram, reset_en, 3, nullptr, 0);
    busy_wait_us(50);
    uint8_t reset[] = {8, 0, 0x99u};
    pio_spi_write_read_dma_blocking(&g_psram, reset, 3, nullptr, 0);
    busy_wait_us(100);
    ok_ = self_test();
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

void Psram::write_word(uint32_t addr, uint32_t value) {
    psram_write32(&g_psram, addr, value);
}

uint32_t Psram::read_word(uint32_t addr) {
    return psram_read32(&g_psram, addr);
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
