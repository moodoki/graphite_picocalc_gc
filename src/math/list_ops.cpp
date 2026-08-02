#include "math/list_ops.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "math/engine.hpp"
#include "math/scratch.hpp"

namespace math::listops {

namespace {

constexpr int kChunk = 256;
constexpr int kMergeBuf = 128;

// Streaming buffers overlay the shared listops region (scratch.hpp). This
// region is DISJOINT from the compute region because list_expr calls
// listops (sum/prod/seq/sort/cumsum/copy...), so a listops buffer can be
// live while list_expr's own buffers are. Single-core, no reentrancy
// within listops.
calc_t (&g_buf)[kChunk] = *reinterpret_cast<calc_t (*)[kChunk]>(scratch::listops_region());
calc_t (&g_in_a)[kMergeBuf] = *reinterpret_cast<calc_t (*)[kMergeBuf]>(scratch::listops_region() +
                                                                       sizeof(g_buf));
calc_t (&g_in_b)[kMergeBuf] = *reinterpret_cast<calc_t (*)[kMergeBuf]>(scratch::listops_region() +
                                                                       sizeof(g_buf) +
                                                                       sizeof(g_in_a));
calc_t (&g_out)[kMergeBuf] = *reinterpret_cast<calc_t (*)[kMergeBuf]>(
    scratch::listops_region() + sizeof(g_buf) + sizeof(g_in_a) + sizeof(g_in_b));
static_assert(sizeof(g_buf) + 3 * sizeof(calc_t) * kMergeBuf <= scratch::kListopsBytes,
              "listops scratch exceeds shared listops region");

// Total order with NaNs last — std::sort on raw operator< is UB when
// NaNs are present (seq() can produce them).
bool less_asc(calc_t a, calc_t b) {
    if (std::isnan(a)) {
        return false;
    }
    if (std::isnan(b)) {
        return true;
    }
    return a < b;
}

bool takes_first(calc_t a, calc_t b, bool asc) {
    return asc ? !less_asc(b, a) : !less_asc(a, b);
}

// One buffered side of a streaming merge over raw PSRAM addresses.
struct RunReader {
    uint32_t base = 0;  // Byte address of element 0 of the run's array
    int pos = 0;        // Next element (flat index)
    int end = 0;
    calc_t* buf = nullptr;
    int buf_at = 0;
    int buf_n = 0;

    bool empty() const { return pos >= end && buf_at >= buf_n; }
    calc_t peek() {
        if (buf_at >= buf_n) {
            buf_n = end - pos < kMergeBuf ? end - pos : kMergeBuf;
            psram_backend::read(base + static_cast<uint32_t>(pos) * sizeof(calc_t), buf,
                                static_cast<size_t>(buf_n) * sizeof(calc_t));
            pos += buf_n;
            buf_at = 0;
        }
        return buf[buf_at];
    }
    calc_t take() {
        const calc_t v = peek();
        ++buf_at;
        return v;
    }
};

// Merge src[lo,mid) + src[mid,hi) (each sorted) into dst[lo,hi).
void merge_runs(uint32_t src, uint32_t dst, int lo, int mid, int hi, bool asc) {
    RunReader a{src, lo, mid, g_in_a};
    RunReader b{src, mid, hi, g_in_b};
    int out_at = lo;
    int out_n = 0;
    auto flush = [&]() {
        if (out_n > 0) {
            psram_backend::write(dst + static_cast<uint32_t>(out_at) * sizeof(calc_t), g_out,
                                 static_cast<size_t>(out_n) * sizeof(calc_t));
            out_at += out_n;
            out_n = 0;
        }
    };
    while (!a.empty() || !b.empty()) {
        calc_t v = 0;
        if (a.empty()) {
            v = b.take();
        } else if (b.empty()) {
            v = a.take();
        } else {
            v = takes_first(a.peek(), b.peek(), asc) ? a.take() : b.take();
        }
        g_out[out_n++] = v;
        if (out_n == kMergeBuf) {
            flush();
        }
    }
    flush();
}

void sort_chunk(calc_t* p, int n, bool asc) {
    if (asc) {
        std::sort(p, p + n, less_asc);
    } else {
        std::sort(p, p + n, [](calc_t a, calc_t b) { return less_asc(b, a); });
    }
}

bool sort_inplace(Array& a, bool asc) {
    if (a.dtype() != Dtype::kDouble) {
        return false;  // No ordering on complex lists (D37); callers give the pointed error
    }
    const int n = a.size();
    if (n <= 1) {
        return true;
    }
    if (!a.in_psram()) {
        a.read_range(0, n, g_buf);
        sort_chunk(g_buf, n, asc);
        a.write_range(0, n, g_buf);
        return true;
    }

    // External merge sort: sorted kChunk runs into a temp region, then
    // merge passes ping-ponging temp <-> the list's own region.
    const uint32_t temp = array_store().region_alloc();
    if (temp == psram_backend::kInvalid) {
        return false;
    }
    const uint32_t base = a.psram_addr();
    for (int at = 0; at < n; at += kChunk) {
        const int m = n - at < kChunk ? n - at : kChunk;
        a.read_range(at, m, g_buf);
        sort_chunk(g_buf, m, asc);
        psram_backend::write(temp + static_cast<uint32_t>(at) * sizeof(calc_t), g_buf,
                             static_cast<size_t>(m) * sizeof(calc_t));
    }
    uint32_t src = temp;
    uint32_t dst = base;
    for (int run = kChunk; run < n; run *= 2) {
        for (int lo = 0; lo < n; lo += 2 * run) {
            const int mid = std::min(lo + run, n);
            const int hi = std::min(lo + 2 * run, n);
            merge_runs(src, dst, lo, mid, hi, asc);
        }
        std::swap(src, dst);
    }
    // The sorted data ends in `src` (post-swap); copy back if needed.
    if (src != base) {
        for (int at = 0; at < n; at += kChunk) {
            const int m = n - at < kChunk ? n - at : kChunk;
            psram_backend::read(src + static_cast<uint32_t>(at) * sizeof(calc_t), g_buf,
                                static_cast<size_t>(m) * sizeof(calc_t));
            psram_backend::write(base + static_cast<uint32_t>(at) * sizeof(calc_t), g_buf,
                                 static_cast<size_t>(m) * sizeof(calc_t));
        }
    }
    array_store().region_free(temp);
    return true;
}

}  // namespace

calc_t sum(const Array& a) {
    if (a.dtype() != Dtype::kDouble) {
        return std::numeric_limits<calc_t>::quiet_NaN();
    }
    calc_t acc = 0;
    const int n = a.size();
    for (int at = 0; at < n; at += kChunk) {
        const int m = n - at < kChunk ? n - at : kChunk;
        a.read_range(at, m, g_buf);
        for (int i = 0; i < m; ++i) {
            acc += g_buf[i];
        }
    }
    return acc;
}

calc_t prod(const Array& a) {
    if (a.dtype() != Dtype::kDouble) {
        return std::numeric_limits<calc_t>::quiet_NaN();
    }
    calc_t acc = 1;
    const int n = a.size();
    for (int at = 0; at < n; at += kChunk) {
        const int m = n - at < kChunk ? n - at : kChunk;
        a.read_range(at, m, g_buf);
        for (int i = 0; i < m; ++i) {
            acc *= g_buf[i];
        }
    }
    return acc;
}

Complex csum(const Array& a) {
    // Element-at-a-time (no complex chunk buffer = no new bss); a
    // complex list caps at 5000 elements, fine for a home-screen op.
    Complex acc(0.0, 0.0);
    const int n = a.size();
    for (int i = 0; i < n; ++i) {
        acc = acc + a.cget(i);
    }
    return acc;
}

bool sort_asc(Array& a) {
    return sort_inplace(a, true);
}

bool sort_desc(Array& a) {
    return sort_inplace(a, false);
}

bool cumsum(const Array& a, Array& out) {
    if (a.dtype() != Dtype::kDouble) {
        return false;  // Real-only (D37); listexpr gives the pointed error
    }
    if (&a == &out) {
        return false;
    }
    const int n = a.size();
    if (!out.resize(n)) {
        return false;
    }
    calc_t carry = 0;
    for (int at = 0; at < n; at += kChunk) {
        const int m = n - at < kChunk ? n - at : kChunk;
        a.read_range(at, m, g_buf);
        for (int i = 0; i < m; ++i) {
            carry += g_buf[i];
            g_buf[i] = carry;
        }
        out.write_range(at, m, g_buf);
    }
    return true;
}

bool delta_list(const Array& a, Array& out) {
    if (a.dtype() != Dtype::kDouble) {
        return false;  // Real-only (D37); listexpr gives the pointed error
    }
    if (&a == &out) {
        return false;
    }
    const int n = a.size();
    if (!out.resize(n > 0 ? n - 1 : 0)) {
        return false;
    }
    calc_t prev = n > 0 ? a.get(0) : 0;
    for (int at = 1; at < n; at += kChunk) {
        const int m = n - at < kChunk ? n - at : kChunk;
        a.read_range(at, m, g_buf);
        for (int i = 0; i < m; ++i) {
            const calc_t cur = g_buf[i];
            g_buf[i] = cur - prev;
            prev = cur;
        }
        out.write_range(at - 1, m, g_buf);
    }
    return true;
}

bool seq(const char* expr, int var_slot, calc_t lo, calc_t hi, calc_t step, Array& out,
         const char** err) {
    *err = nullptr;
    if (step == 0 || std::isnan(step) || std::isnan(lo) || std::isnan(hi)) {
        *err = "Bad seq step";
        return false;
    }
    const calc_t span = (hi - lo) / step;
    if (span < 0) {
        *err = "Bad seq range";
        return false;
    }
    // Half-step tolerance so seq(x, x, 0, 1, 0.1) lands on 1.0 despite
    // binary-fraction rounding.
    const int count = static_cast<int>(std::floor(span + 0.5 * 1e-9 + 1e-9)) + 1;
    if (count > Array::kMaxElements) {
        *err = "List too long (max 10000)";
        return false;
    }
    Engine& eng = engine();
    void* h = eng.compile(expr, var_slot);
    out.clear();
    out.set_dtype(Dtype::kDouble);  // seq output is always real
    if (h == nullptr) {
        *err = "Syntax error in seq formula";
        return false;
    }
    if (!out.resize(count)) {
        eng.free_compiled(h);
        *err = "Out of list memory";
        return false;
    }
    const calc_t saved = eng.vars().vars[var_slot];
    for (int at = 0; at < count; at += kChunk) {
        const int m = count - at < kChunk ? count - at : kChunk;
        for (int i = 0; i < m; ++i) {
            g_buf[i] = eng.eval_compiled(h, var_slot, lo + static_cast<calc_t>(at + i) * step);
        }
        out.write_range(at, m, g_buf);
    }
    eng.vars().vars[var_slot] = saved;
    eng.free_compiled(h);
    return true;
}

bool copy(const Array& src, Array& dst) {
    if (&src == &dst) {
        return true;
    }
    if (dst.dtype() != src.dtype()) {
        dst.clear();
        if (!dst.set_dtype(src.dtype())) {
            return false;
        }
    }
    const int n = src.size();
    if (!dst.resize(n)) {
        return false;
    }
    if (src.dtype() == Dtype::kComplex) {
        for (int i = 0; i < n; ++i) {
            dst.cset(i, src.cget(i));
        }
        return true;
    }
    for (int at = 0; at < n; at += kChunk) {
        const int m = n - at < kChunk ? n - at : kChunk;
        src.read_range(at, m, g_buf);
        dst.write_range(at, m, g_buf);
    }
    return true;
}

bool make_complex(Array& a) {
    if (a.dtype() == Dtype::kComplex) {
        return true;
    }
    static Array staging;
    if (!copy_complex(a, staging) || !copy(staging, a)) {
        staging.clear();
        return false;
    }
    staging.clear();
    return true;
}

bool copy_complex(const Array& src, Array& dst) {
    if (&src == &dst) {
        return src.dtype() == Dtype::kComplex;
    }
    dst.clear();
    if (!dst.set_dtype(Dtype::kComplex)) {
        return false;
    }
    const int n = src.size();
    if (!dst.resize(n)) {
        return false;
    }
    for (int i = 0; i < n; ++i) {
        dst.cset(i, src.cget(i));
    }
    return true;
}

}  // namespace math::listops
