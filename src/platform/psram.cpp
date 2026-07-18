#include "platform/psram.hpp"

#include <cstring>

extern "C" {
#include "rp2040-psram/psram_spi.h"
}

namespace platform {

namespace {
psram_spi_inst_t g_psram;

// Bulk-transfer chunk caps (D10 fix, 2026-07-18). The PIO program takes
// 8-bit transfer counts ("out x, 8" / "out y, 8"), so one transaction
// moves at most 255 bits: 31 bytes total. A write spends 4 of those on
// command+address (27 data bytes); a read's command phase is counted
// separately (31 data bytes). The vendored psram_write()/psram_read()
// let the count byte wrap above that — (4+count)*8 mod 256 — which
// desyncs the PIO from the DMA byte stream and wedges the blocking
// DMA wait (the Phase 1 boot hang). Chunking this size also keeps CS
// low well under the PSRAM's ~8 us tCEM cap (~4 us per chunk).
constexpr size_t kWriteChunk = 27;
constexpr size_t kReadChunk = 31;

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
    // One buffer per chunk (header + payload) so each chunk is a single
    // DMA call under one mutex hold; blocking, so the stack buffer is
    // safe as a DMA source.
    uint8_t cmd[6 + kWriteChunk];
    while (len > 0) {
        const size_t n = len < kWriteChunk ? len : kWriteChunk;
        cmd[0] = static_cast<uint8_t>((4 + n) * 8);  // bits out
        cmd[1] = 0;                                  // bits in
        cmd[2] = 0x02u;                              // Write command
        cmd[3] = static_cast<uint8_t>(addr >> 16);
        cmd[4] = static_cast<uint8_t>(addr >> 8);
        cmd[5] = static_cast<uint8_t>(addr);
        std::memcpy(cmd + 6, data, n);
        pio_spi_write_dma_blocking(&g_psram, cmd, 6 + n);
        addr += n;
        data += n;
        len -= n;
    }
}

void Psram::read(uint32_t addr, uint8_t* data, size_t len) {
    while (len > 0) {
        const size_t n = len < kReadChunk ? len : kReadChunk;
        // 40 bits out: 0x0B fast-read + 3 address bytes + 8 dummy cycles.
        const uint8_t cmd[7] = {40,
                                static_cast<uint8_t>(n * 8),
                                0x0bu,
                                static_cast<uint8_t>(addr >> 16),
                                static_cast<uint8_t>(addr >> 8),
                                static_cast<uint8_t>(addr),
                                0};
        pio_spi_write_read_dma_blocking(&g_psram, cmd, sizeof(cmd), data, n);
        addr += n;
        data += n;
        len -= n;
    }
}

Psram& psram() {
    static Psram instance;
    return instance;
}

}  // namespace platform
