#pragma once

#include <cstddef>
#include <cstdint>

#include "math/types.hpp"

namespace math {

// Element type tag (D21): double-only today, but persisted and routed
// through every accessor so complex-valued lists/matrices (committed
// future scope) land as a non-breaking addition.
enum class Dtype : uint8_t { kDouble = 0 };

constexpr size_t dtype_size(Dtype t) {
    return t == Dtype::kDouble ? sizeof(calc_t) : 0;
}

// Low-level PSRAM hooks for the large-array tier. The firmware
// implementation wraps platform::psram() (array_backend_pico.cpp);
// host tests link a malloc shim instead (tests/host provides it), so
// everything above this seam is host-testable.
namespace psram_backend {

constexpr uint32_t kInvalid = 0xFFFFFFFFu;

bool available();
// Bump allocation (no free) — ArrayStore recycles fixed-size regions
// on top of this, so the bump pointer only ever moves for new regions.
uint32_t alloc(size_t bytes);
void read(uint32_t addr, void* out, size_t len);
void write(uint32_t addr, const void* src, size_t len);

}  // namespace psram_backend

// N-dimensional numeric array (spec §2, shaped by D21/D22). Row-major.
// Lists are 1-D (shape {n}); Phase 4 matrices are 2-D ({rows, cols}).
//
// Storage is tiered (D21): arrays whose payload fits one SRAM slab
// (<= 256 doubles) live in an SRAM pool; larger ones live in a fixed
// 10000-double PSRAM region. PSRAM is SPI-attached and NOT memory
// mapped, so — deviating from the spec's sketch — there are no
// reference-returning accessors and no data() pointer: element access
// is get()/set(), bulk access is read_range()/write_range() (chunked
// DMA underneath, ~6.8 MB/s per D10).
class Array {
public:
    static constexpr int kMaxDims = 2;
    static constexpr int kMaxElements = 10000;  // D21 cap

    Array() = default;
    ~Array() { clear(); }
    Array(const Array&) = delete;
    Array& operator=(const Array&) = delete;

    int ndim() const { return ndim_; }
    int dim(int axis) const { return (axis >= 0 && axis < kMaxDims) ? shape_[axis] : 0; }
    int size() const;
    bool is_list() const { return ndim_ == 1; }
    bool is_matrix() const { return ndim_ == 2; }
    Dtype dtype() const { return dtype_; }
    bool in_psram() const { return psram_addr_ != kNoPsram; }

    // Element access, routed through the dtype tag (D21). Out-of-range
    // get returns NaN; out-of-range set is a no-op.
    calc_t get(int i) const;
    calc_t get(int r, int c) const { return get(flat(r, c)); }
    void set(int i, calc_t v);
    void set(int r, int c, calc_t v) { set(flat(r, c), v); }

    // Bulk element ranges (row-major flat indices, clamped to bounds).
    void read_range(int first, int count, calc_t* out) const;
    void write_range(int first, int count, const calc_t* src);

    void fill(calc_t v);

    // Resize, preserving the leading elements; new elements are zero.
    // Returns false (array unchanged) when out of range or the PSRAM
    // tier is needed but unavailable (cold boot, D14 — retry later).
    bool resize(int d0);
    bool resize(int d0, int d1);

    void clear();  // Size 0, releases all storage back to the store

    // PSRAM region base for list_ops' external merge sort. kInvalid
    // for the SRAM tier. Not for general use.
    uint32_t psram_addr() const { return psram_addr_; }

private:
    static constexpr uint32_t kNoPsram = psram_backend::kInvalid;

    int ndim_ = 0;
    int shape_[kMaxDims] = {0, 0};
    Dtype dtype_ = Dtype::kDouble;
    uint8_t* sram_ = nullptr;
    uint32_t psram_addr_ = kNoPsram;

    int flat(int r, int c) const {
        return (ndim_ == 2 && r >= 0 && c >= 0 && c < shape_[1]) ? r * shape_[1] + c : -1;
    }
    bool set_shape(int nd, int d0, int d1);
};

// Backing store (spec §2.2): a fixed pool of 2 KB SRAM slabs (one slab
// == one small array) and a free-list of fixed-size PSRAM regions over
// the platform bump allocator. Fixed sizes mean recycling is trivial
// and fragmentation impossible; the bump pointer only grows until
// kMaxPsramRegions distinct regions exist.
class ArrayStore {
public:
    static constexpr size_t kSlabBytes = 2048;  // 256 doubles (D21 threshold)
    static constexpr int kSlabCount = 12;       // 6 lists + expression temps
    static constexpr size_t kRegionBytes =
        static_cast<size_t>(Array::kMaxElements) * sizeof(calc_t);
    static constexpr int kMaxPsramRegions = 12;

    uint8_t* slab_alloc();
    void slab_free(const uint8_t* p);
    uint32_t region_alloc();  // psram_backend::kInvalid on failure
    void region_free(uint32_t addr);

    size_t sram_used() const;
    size_t psram_used() const;

private:
    uint8_t slabs_[kSlabCount][kSlabBytes];
    bool slab_used_[kSlabCount] = {};
    uint32_t regions_[kMaxPsramRegions] = {};
    bool region_exists_[kMaxPsramRegions] = {};
    bool region_used_[kMaxPsramRegions] = {};
};

// Singleton accessor (project convention — matches math::engine()).
ArrayStore& array_store();

}  // namespace math
