#pragma once

#include <cstddef>
#include <cstdint>

#include "math/complex.hpp"
#include "math/types.hpp"

namespace math {

// Element type tag (D21): kDouble is the default; kComplex (4D.24,
// D37/D38) stores interleaved re/im pairs. Complex arrays route
// exclusively through the PSRAM region tier — never the SRAM slab pool
// — so the committed bss budget stays real-only (D37).
enum class Dtype : uint8_t { kDouble = 0, kComplex = 1 };

constexpr size_t dtype_size(Dtype t) {
    return t == Dtype::kDouble ? sizeof(calc_t) : t == Dtype::kComplex ? 2 * sizeof(calc_t) : 0;
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
    // Complex arrays cap at half the elements so one 80 KB PSRAM
    // region still holds a full array (16 bytes/element, 4D.24).
    static constexpr int kMaxComplexElements = kMaxElements / 2;

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
    // get returns NaN; out-of-range set is a no-op. The calc_t
    // accessors are kDouble-only: on a kComplex array get() returns NaN
    // and set()/the ranges are no-ops — real-only consumers must check
    // dtype and error, never silently read a real part (D37).
    calc_t get(int i) const;
    calc_t get(int r, int c) const { return get(flat(r, c)); }
    void set(int i, calc_t v);
    void set(int r, int c, calc_t v) { set(flat(r, c), v); }

    // Bulk element ranges (row-major flat indices, clamped to bounds).
    void read_range(int first, int count, calc_t* out) const;
    void write_range(int first, int count, const calc_t* src);

    // Complex element access (4D.24). cget works on both dtypes
    // (kDouble promotes to {v, 0}); cset and the ranges are
    // kComplex-only no-ops otherwise.
    Complex cget(int i) const;
    Complex cget(int r, int c) const { return cget(flat(r, c)); }
    void cset(int i, const Complex& v);
    void cset(int r, int c, const Complex& v) { cset(flat(r, c), v); }
    void read_range_c(int first, int count, Complex* out) const;
    void write_range_c(int first, int count, const Complex* src);

    // Change the element type. Only valid on an empty array (clear()
    // or resize(0) first); the caller then resizes and fills.
    bool set_dtype(Dtype t);

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
    // 6 lists + list expression temps, plus (4A) 10 matrices + MatAns +
    // matrix expression/work temps. Slabs are SRAM (kSlabCount * 2 KB
    // of bss); regions are PSRAM bookkeeping only — a region's 80 KB
    // is bump-allocated on first use.
    //
    // 28 -> 14 (D70 lever B, 2026-08-15): -28 KB of bss. Sized from a
    // device measurement, not a guess — instrumented hardware runs
    // peaked at **12** live slabs across lists, matrices, list
    // arithmetic, cumsum and stats, with 11 live in steady state, so 14
    // keeps ordinary use entirely in SRAM with margin.
    //
    // Exhaustion is no longer fatal: slab_alloc() returning null now
    // makes the caller fall back to PSRAM (see Array::set_shape), which
    // was verified on hardware at a deliberately hostile kSlabCount = 4
    // — 11 fallbacks fired, every result stayed correct, and a
    // 30-repeat sensitive check came back 30/30 clean.
    //
    // Cutting to 8 would return a further 12 KB and does work, but
    // steady-state live is 11, so the calculator would permanently run
    // several of its built-in lists out of PSRAM. That trades ordinary-
    // path speed and extra exposure to D53 (open, un-root-caused
    // intermittent PSRAM read fault) for headroom the budget does not
    // need. Revisit only if 6B's own growth actually demands it.
    static constexpr int kSlabCount = 14;
    static constexpr size_t kRegionBytes =
        static_cast<size_t>(Array::kMaxElements) * sizeof(calc_t);
    static constexpr int kMaxPsramRegions = 24;

    // Null when the pool is exhausted. Callers must treat that as
    // "use PSRAM instead", not as an error (D70 lever B) — the pool is
    // sized to the common case, not the worst one.
    uint8_t* slab_alloc();
    void slab_free(const uint8_t* p);
    uint32_t region_alloc();  // psram_backend::kInvalid on failure
    void region_free(uint32_t addr);

    size_t sram_used() const;
    size_t psram_used() const;

    // Instrumentation for sizing kSlabCount from a device measurement
    // rather than a guess. peak = high-water live slabs; misses = times
    // the pool was exhausted and a caller fell back to PSRAM.
    int slabs_peak() const { return slabs_peak_; }
    int slabs_live() const { return slabs_live_; }
    uint32_t slab_misses() const { return slab_misses_; }

private:
    uint8_t slabs_[kSlabCount][kSlabBytes];
    bool slab_used_[kSlabCount] = {};
    int slabs_live_ = 0;
    int slabs_peak_ = 0;
    uint32_t slab_misses_ = 0;
    uint32_t regions_[kMaxPsramRegions] = {};
    bool region_exists_[kMaxPsramRegions] = {};
    bool region_used_[kMaxPsramRegions] = {};
};

// Singleton accessor (project convention — matches math::engine()).
ArrayStore& array_store();

}  // namespace math
