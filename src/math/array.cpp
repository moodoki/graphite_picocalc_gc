#include "math/array.hpp"

#include <cmath>
#include <cstring>
#include <limits>

namespace math {

namespace {

constexpr int kSlabElements = static_cast<int>(ArrayStore::kSlabBytes / sizeof(calc_t));

// Chunk buffer for PSRAM-tier fills and tier migrations. Single-core
// application code — no reentrancy concern.
calc_t g_chunk[kSlabElements];

}  // namespace

int Array::size() const {
    if (ndim_ == 0) {
        return 0;
    }
    return ndim_ == 1 ? shape_[0] : shape_[0] * shape_[1];
}

calc_t Array::get(int i) const {
    if (i < 0 || i >= size()) {
        return std::numeric_limits<calc_t>::quiet_NaN();
    }
    // dtype-aware access (D21): kDouble is the only tag today.
    calc_t v = 0;
    const size_t off = static_cast<size_t>(i) * dtype_size(dtype_);
    if (in_psram()) {
        psram_backend::read(psram_addr_ + off, &v, sizeof(v));
    } else {
        std::memcpy(&v, sram_ + off, sizeof(v));
    }
    return v;
}

void Array::set(int i, calc_t v) {
    if (i < 0 || i >= size()) {
        return;
    }
    const size_t off = static_cast<size_t>(i) * dtype_size(dtype_);
    if (in_psram()) {
        psram_backend::write(psram_addr_ + off, &v, sizeof(v));
    } else {
        std::memcpy(sram_ + off, &v, sizeof(v));
    }
}

void Array::read_range(int first, int count, calc_t* out) const {
    if (first < 0 || count <= 0 || first + count > size()) {
        return;
    }
    const size_t off = static_cast<size_t>(first) * dtype_size(dtype_);
    const size_t bytes = static_cast<size_t>(count) * dtype_size(dtype_);
    if (in_psram()) {
        psram_backend::read(psram_addr_ + off, out, bytes);
    } else {
        std::memcpy(out, sram_ + off, bytes);
    }
}

void Array::write_range(int first, int count, const calc_t* src) {
    if (first < 0 || count <= 0 || first + count > size()) {
        return;
    }
    const size_t off = static_cast<size_t>(first) * dtype_size(dtype_);
    const size_t bytes = static_cast<size_t>(count) * dtype_size(dtype_);
    if (in_psram()) {
        psram_backend::write(psram_addr_ + off, src, bytes);
    } else {
        std::memcpy(sram_ + off, src, bytes);
    }
}

void Array::fill(calc_t v) {
    for (calc_t& c : g_chunk) {
        c = v;
    }
    const int n = size();
    for (int at = 0; at < n; at += kSlabElements) {
        const int m = n - at < kSlabElements ? n - at : kSlabElements;
        write_range(at, m, g_chunk);
    }
}

// Move storage to the tier fitting `elements`, preserving the leading
// min(old, new) elements and zero-filling any growth.
bool Array::set_shape(int nd, int d0, int d1) {
    const int new_size = nd == 1 ? d0 : d0 * d1;
    if (d0 < 0 || d1 < 0 || new_size > kMaxElements) {
        return false;
    }
    const int old_size = size();
    const size_t new_bytes = static_cast<size_t>(new_size) * dtype_size(dtype_);
    const bool want_psram = new_bytes > ArrayStore::kSlabBytes;

    if (new_size == 0) {
        clear();
        ndim_ = nd;
        shape_[0] = d0;
        shape_[1] = nd == 2 ? d1 : 0;
        return true;
    }

    if (want_psram && !in_psram()) {
        if (!psram_backend::available()) {
            return false;
        }
        const uint32_t region = array_store().region_alloc();
        if (region == psram_backend::kInvalid) {
            return false;
        }
        if (sram_ != nullptr && old_size > 0) {
            psram_backend::write(region, sram_, static_cast<size_t>(old_size) * dtype_size(dtype_));
        }
        if (sram_ != nullptr) {
            array_store().slab_free(sram_);
            sram_ = nullptr;
        }
        psram_addr_ = region;
    } else if (!want_psram && in_psram()) {
        // Shrink back into a slab, releasing the 80 KB region.
        uint8_t* slab = array_store().slab_alloc();
        if (slab == nullptr) {
            return false;  // Pool exhausted; keep the PSRAM tier as-is
        }
        const int keep = old_size < new_size ? old_size : new_size;
        if (keep > 0) {
            psram_backend::read(psram_addr_, slab, static_cast<size_t>(keep) * dtype_size(dtype_));
        }
        array_store().region_free(psram_addr_);
        psram_addr_ = kNoPsram;
        sram_ = slab;
    } else if (!want_psram && sram_ == nullptr) {
        sram_ = array_store().slab_alloc();
        if (sram_ == nullptr) {
            return false;
        }
    }

    ndim_ = nd;
    shape_[0] = d0;
    shape_[1] = nd == 2 ? d1 : 0;

    if (new_size > old_size) {
        for (calc_t& c : g_chunk) {
            c = 0;
        }
        for (int at = old_size; at < new_size; at += kSlabElements) {
            const int m = new_size - at < kSlabElements ? new_size - at : kSlabElements;
            write_range(at, m, g_chunk);
        }
    }
    return true;
}

bool Array::resize(int d0) {
    return set_shape(1, d0, 0);
}

bool Array::resize(int d0, int d1) {
    return set_shape(2, d0, d1);
}

void Array::clear() {
    if (sram_ != nullptr) {
        array_store().slab_free(sram_);
        sram_ = nullptr;
    }
    if (psram_addr_ != kNoPsram) {
        array_store().region_free(psram_addr_);
        psram_addr_ = kNoPsram;
    }
    ndim_ = 0;
    shape_[0] = 0;
    shape_[1] = 0;
}

uint8_t* ArrayStore::slab_alloc() {
    for (int i = 0; i < kSlabCount; ++i) {
        if (!slab_used_[i]) {
            slab_used_[i] = true;
            return slabs_[i];
        }
    }
    return nullptr;
}

void ArrayStore::slab_free(const uint8_t* p) {
    for (int i = 0; i < kSlabCount; ++i) {
        if (slabs_[i] == p) {
            slab_used_[i] = false;
            return;
        }
    }
}

uint32_t ArrayStore::region_alloc() {
    for (int i = 0; i < kMaxPsramRegions; ++i) {
        if (region_exists_[i] && !region_used_[i]) {
            region_used_[i] = true;
            return regions_[i];
        }
    }
    for (int i = 0; i < kMaxPsramRegions; ++i) {
        if (!region_exists_[i]) {
            const uint32_t addr = psram_backend::alloc(kRegionBytes);
            if (addr == psram_backend::kInvalid) {
                return psram_backend::kInvalid;
            }
            regions_[i] = addr;
            region_exists_[i] = true;
            region_used_[i] = true;
            return addr;
        }
    }
    return psram_backend::kInvalid;
}

void ArrayStore::region_free(uint32_t addr) {
    for (int i = 0; i < kMaxPsramRegions; ++i) {
        if (region_exists_[i] && regions_[i] == addr) {
            region_used_[i] = false;
            return;
        }
    }
}

size_t ArrayStore::sram_used() const {
    size_t n = 0;
    for (const bool used : slab_used_) {
        n += used ? kSlabBytes : 0;
    }
    return n;
}

size_t ArrayStore::psram_used() const {
    size_t n = 0;
    for (const bool used : region_used_) {
        n += used ? kRegionBytes : 0;
    }
    return n;
}

ArrayStore& array_store() {
    static ArrayStore instance;
    return instance;
}

}  // namespace math
