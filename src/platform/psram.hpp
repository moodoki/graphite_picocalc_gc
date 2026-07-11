#pragma once

#include <cstddef>
#include <cstdint>

namespace platform {

// 8 MB SPI PSRAM (PIO-driven, vendored rp2040-psram driver).
//
// Simple bump allocator over the PSRAM address space: alloc() hands out
// *PSRAM addresses* (offsets), not host pointers — PSRAM is not memory
// mapped, so access goes through read()/write(). reset() frees all.
class Psram {
public:
    // Returns false if the PSRAM self-test fails (hardware absent, or
    // the peripheral rail is still settling on a cold RP2350 boot).
    bool init();

    // Re-attempt bring-up after a failed init(): re-sends the chip
    // reset and re-runs the self-test without re-allocating PIO/DMA
    // resources. On a cold Pico 2 power-on the PSRAM needs several
    // seconds of rail settle (D14) — the main loop retries via this.
    bool reinit();

    bool ok() const { return ok_; }

    static constexpr uint32_t kInvalid = 0xFFFFFFFF;

    // Allocate a region, returns PSRAM address or kInvalid when full.
    uint32_t alloc(size_t bytes, size_t alignment = 4);
    void reset();

    // Single-word access (verified working on hardware 2026-07-10).
    void write_word(uint32_t addr, uint32_t value);
    uint32_t read_word(uint32_t addr);

    // Bulk access. WARNING: the vendored bulk transfer path hangs on
    // PicoCalc hardware (2026-07-10) — do NOT use until fixed. Phase 1
    // does not need it (framebuffer is line-buffered in SRAM). See D10 /
    // worklog. Left declared for the future statistics/matrix use cases.
    void write(uint32_t addr, const uint8_t* data, size_t len);
    void read(uint32_t addr, uint8_t* data, size_t len);

    size_t total_bytes() const { return 8u * 1024 * 1024; }
    size_t used_bytes() const { return next_; }
    size_t free_bytes() const { return total_bytes() - next_; }

private:
    bool ok_ = false;
    uint32_t next_ = 0;
};

Psram& psram();

}  // namespace platform
