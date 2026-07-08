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
    // Returns false if the PSRAM self-test fails (hardware absent).
    bool init();

    bool ok() const { return ok_; }

    static constexpr uint32_t kInvalid = 0xFFFFFFFF;

    // Allocate a region, returns PSRAM address or kInvalid when full.
    uint32_t alloc(size_t bytes, size_t alignment = 4);
    void reset();

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
