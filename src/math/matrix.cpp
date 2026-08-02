#include "math/matrix.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>

#include "math/scratch.hpp"

namespace math::matops {

namespace {

// Streaming row buffers. Never more than three rows are in flight at
// once (e.g. mul: one A row, one B row, one accumulator row). Complex
// kernels (4D.25) view the same storage as Complex rows through a
// union, so the complex tier costs one buffer set, not two (the D37
// "bss stays real-only" budget bends by the 2x element width only).
union RowBuf {
    calc_t d[kMaxRowElems];
    Complex c[kMaxRowElems];
    // Complex's default ctor is non-trivial (member initializers), so
    // the union needs a user-provided one; start life as the real view.
    RowBuf() : d{} {}
};
// The three row buffers overlay the shared compute region (scratch.hpp):
// matops is mutually exclusive with list_expr/stats/infer, so it reuses the
// same bytes. RowBuf has a non-trivial ctor (above), so placement-new into
// the arena to start each object's lifetime properly (runs once at startup,
// on already-zeroed bss). Never more than these three are live at once.
RowBuf& make_rowbuf(std::size_t off) {
    return *new (scratch::compute_region() + off) RowBuf();
}
RowBuf& g_rowa = make_rowbuf(0);
RowBuf& g_rowb = make_rowbuf(sizeof(RowBuf));
RowBuf& g_rowc = make_rowbuf(2 * sizeof(RowBuf));
static_assert(3 * sizeof(RowBuf) <= scratch::kComputeBytes,
              "matops row buffers exceed shared compute region");

// Working copies for the elimination algorithms and power().
Array g_mwork[2];
// make_complex() staging (never aliases the matrix being migrated).
Array g_staging;

const char* const kErrNotMatrix = "Not a matrix";
const char* const kErrDim = "Dim mismatch";
const char* const kErrNotSquare = "Not square";
const char* const kErrTooLarge = "Matrix too large";
const char* const kErrMemory = "Out of matrix memory";
const char* const kErrSingular = "Singular matrix";
const char* const kErrComplexMat = "Non-real matrix";

bool valid(const Array& a) {
    return a.is_matrix() && a.dim(0) >= 1 && a.dim(1) >= 1;
}

bool dims_ok(int rows, int cols) {
    return rows >= 1 && cols >= 1 && rows <= kMaxRowElems && cols <= kMaxRowElems &&
           rows * cols <= Array::kMaxElements;
}

// Retype an output array (expression temps are recycled and may carry
// a stale dtype from the previous expression).
bool retype(Array& out, Dtype t) {
    if (out.dtype() == t) {
        return true;
    }
    out.clear();
    return out.set_dtype(t);
}

Dtype join(const Array& a, const Array& b) {
    return (a.dtype() == Dtype::kComplex || b.dtype() == Dtype::kComplex) ? Dtype::kComplex
                                                                          : Dtype::kDouble;
}

// ---- Element-type shims (4D.25) ----
//
// The row-streaming kernels below are templated on one of these two
// policies. RealOps is the original calc_t fast path; CplxOps reads
// promote a kDouble source to Complex on the fly (mixed real/complex
// operands), and pivoting magnitude is the modulus.

struct RealOps {
    using T = calc_t;
    static T* elems(RowBuf& b) { return b.d; }
    static T at(const Array& a, int r, int c) { return a.get(r, c); }
    static void put(Array& a, int r, int c, T v) { a.set(r, c, v); }
    static void read(const Array& a, int first, int count, RowBuf& b) {
        a.read_range(first, count, b.d);
    }
    static void write(Array& a, int first, int count, const RowBuf& b) {
        a.write_range(first, count, b.d);
    }
    static calc_t mag(T v) { return std::fabs(v); }
    static T one() { return 1.0; }
};

struct CplxOps {
    using T = Complex;
    static T* elems(RowBuf& b) { return b.c; }
    static T at(const Array& a, int r, int c) { return a.cget(r, c); }
    static void put(Array& a, int r, int c, const T& v) { a.cset(r, c, v); }
    static void read(const Array& a, int first, int count, RowBuf& b) {
        if (a.dtype() == Dtype::kComplex) {
            a.read_range_c(first, count, b.c);
            return;
        }
        // Promote a real range in place: widen back-to-front so no
        // source element is overwritten before it has been read.
        a.read_range(first, count, b.d);
        for (int i = count - 1; i >= 0; --i) {
            b.c[i] = Complex(b.d[i]);
        }
    }
    static void write(Array& a, int first, int count, const RowBuf& b) {
        a.write_range_c(first, count, b.c);
    }
    static calc_t mag(const T& v) { return v.modulus(); }
    static T one() { return {1.0, 0.0}; }
};

// Pivot tolerance, relative to the matrix's largest magnitude (so
// scaling A by 1e-6 doesn't turn it "singular").
template <typename Ops>
calc_t pivot_eps_t(const Array& a) {
    calc_t maxabs = 0;
    const int n = a.size();
    for (int at = 0; at < n; at += kMaxRowElems) {
        const int m = n - at < kMaxRowElems ? n - at : kMaxRowElems;
        Ops::read(a, at, m, g_rowa);
        const auto* p = Ops::elems(g_rowa);
        for (int i = 0; i < m; ++i) {
            maxabs = std::max(Ops::mag(p[i]), maxabs);
        }
    }
    return 1e-12 * (maxabs > 1.0 ? maxabs : 1.0);
}

template <typename Ops>
void read_row_t(const Array& a, int r, RowBuf& buf) {
    Ops::read(a, r * a.dim(1), a.dim(1), buf);
}

template <typename Ops>
void write_row_t(Array& a, int r, const RowBuf& buf) {
    Ops::write(a, r * a.dim(1), a.dim(1), buf);
}

template <typename Ops>
void swap_rows_t(Array& a, int r1, int r2) {
    if (r1 == r2) {
        return;
    }
    read_row_t<Ops>(a, r1, g_rowa);
    read_row_t<Ops>(a, r2, g_rowb);
    write_row_t<Ops>(a, r1, g_rowb);
    write_row_t<Ops>(a, r2, g_rowa);
}

// a[dst,:] += factor * a[src,:]
template <typename Ops>
void add_scaled_row_t(Array& a, int dst, int src, const typename Ops::T& factor) {
    const int cols = a.dim(1);
    read_row_t<Ops>(a, dst, g_rowa);
    read_row_t<Ops>(a, src, g_rowb);
    auto* pa = Ops::elems(g_rowa);
    auto* pb = Ops::elems(g_rowb);
    for (int c = 0; c < cols; ++c) {
        pa[c] = pa[c] + factor * pb[c];
    }
    write_row_t<Ops>(a, dst, g_rowa);
}

template <typename Ops>
void scale_row_t(Array& a, int r, const typename Ops::T& factor) {
    const int cols = a.dim(1);
    read_row_t<Ops>(a, r, g_rowa);
    auto* pa = Ops::elems(g_rowa);
    for (int c = 0; c < cols; ++c) {
        pa[c] = pa[c] * factor;
    }
    write_row_t<Ops>(a, r, g_rowa);
}

template <typename Ops>
void elementwise_t(const Array& a, const Array& b, Array& out, bool subtract) {
    const int rows = a.dim(0);
    const int cols = a.dim(1);
    for (int r = 0; r < rows; ++r) {
        read_row_t<Ops>(a, r, g_rowa);
        read_row_t<Ops>(b, r, g_rowb);
        auto* pa = Ops::elems(g_rowa);
        auto* pb = Ops::elems(g_rowb);
        for (int c = 0; c < cols; ++c) {
            pa[c] = subtract ? pa[c] - pb[c] : pa[c] + pb[c];
        }
        write_row_t<Ops>(out, r, g_rowa);
    }
}

// C[i,:] = sum_k A[i,k] * B[k,:] — row-major streaming friendly for
// the PSRAM tier (no strided column reads).
template <typename Ops>
void mul_t(const Array& a, const Array& b, Array& out) {
    const int rows = a.dim(0);
    const int inner = a.dim(1);
    const int cols = b.dim(1);
    for (int i = 0; i < rows; ++i) {
        read_row_t<Ops>(a, i, g_rowa);
        auto* pa = Ops::elems(g_rowa);
        auto* pc = Ops::elems(g_rowc);
        for (int c = 0; c < cols; ++c) {
            pc[c] = typename Ops::T{};
        }
        for (int k = 0; k < inner; ++k) {
            if (Ops::mag(pa[k]) == 0) {
                continue;
            }
            read_row_t<Ops>(b, k, g_rowb);
            auto* pb = Ops::elems(g_rowb);
            for (int c = 0; c < cols; ++c) {
                pc[c] = pc[c] + pa[k] * pb[c];
            }
        }
        write_row_t<Ops>(out, i, g_rowc);
    }
}

// Read a source row, scatter it as a destination column. The scatter
// is per-element, but each element is touched exactly once.
template <typename Ops>
void transpose_t(const Array& a, Array& out) {
    const int rows = a.dim(0);
    const int cols = a.dim(1);
    for (int r = 0; r < rows; ++r) {
        read_row_t<Ops>(a, r, g_rowa);
        const auto* pa = Ops::elems(g_rowa);
        for (int c = 0; c < cols; ++c) {
            Ops::put(out, c, r, pa[c]);
        }
    }
}

template <typename Ops>
void augment_t(const Array& a, const Array& b, Array& out) {
    const int rows = a.dim(0);
    const int ca = a.dim(1);
    const int cb = b.dim(1);
    for (int r = 0; r < rows; ++r) {
        read_row_t<Ops>(a, r, g_rowc);
        read_row_t<Ops>(b, r, g_rowb);
        auto* pc = Ops::elems(g_rowc);
        const auto* pb = Ops::elems(g_rowb);
        for (int i = 0; i < cb; ++i) {
            pc[ca + i] = pb[i];
        }
        Ops::write(out, r * (ca + cb), ca + cb, g_rowc);
    }
}

template <typename Ops>
void reshape_t(const Array& a, int rows, int cols, Array& out) {
    // Zero-fill through the row buffer (Array::fill is real-only).
    auto* pb = Ops::elems(g_rowb);
    for (int i = 0; i < kMaxRowElems; ++i) {
        pb[i] = typename Ops::T{};
    }
    const int n = out.size();
    for (int at = 0; at < n; at += kMaxRowElems) {
        Ops::write(out, at, n - at < kMaxRowElems ? n - at : kMaxRowElems, g_rowb);
    }
    if (a.size() == 0 || !a.is_matrix()) {
        return;
    }
    const int copy_rows = rows < a.dim(0) ? rows : a.dim(0);
    const int copy_cols = cols < a.dim(1) ? cols : a.dim(1);
    for (int r = 0; r < copy_rows; ++r) {
        Ops::read(a, r * a.dim(1), copy_cols, g_rowa);
        Ops::write(out, r * cols, copy_cols, g_rowa);
    }
}

// out := I_n (out already sized n x n with the right dtype).
template <typename Ops>
void fill_identity_t(Array& out, int n) {
    auto* pa = Ops::elems(g_rowa);
    for (int c = 0; c < n; ++c) {
        pa[c] = typename Ops::T{};
    }
    for (int r = 0; r < n; ++r) {
        if (r > 0) {
            pa[r - 1] = typename Ops::T{};
        }
        pa[r] = Ops::one();
        write_row_t<Ops>(out, r, g_rowa);
    }
}

// Forward elimination to (unreduced) row echelon form, in place.
// Returns the pivot count; *sign flips per row swap (for det).
template <typename Ops>
int forward_eliminate_t(Array& w, calc_t eps, int* sign) {
    const int rows = w.dim(0);
    const int cols = w.dim(1);
    int pivots = 0;
    for (int col = 0; col < cols && pivots < rows; ++col) {
        int best = -1;
        calc_t best_abs = eps;
        for (int r = pivots; r < rows; ++r) {
            const calc_t v = Ops::mag(Ops::at(w, r, col));
            if (v > best_abs) {
                best_abs = v;
                best = r;
            }
        }
        if (best < 0) {
            continue;  // No pivot in this column
        }
        if (best != pivots && sign != nullptr) {
            *sign = -*sign;
        }
        swap_rows_t<Ops>(w, best, pivots);
        const typename Ops::T piv = Ops::at(w, pivots, col);
        for (int r = pivots + 1; r < rows; ++r) {
            const typename Ops::T v = Ops::at(w, r, col);
            if (Ops::mag(v) != 0) {
                add_scaled_row_t<Ops>(w, r, pivots, -(v / piv));
            }
        }
        ++pivots;
    }
    return pivots;
}

template <typename Ops>
bool det_t(const Array& a, typename Ops::T* out, const char** err) {
    const int n = a.dim(0);
    if (n <= 3) {  // Direct expansion — no working copy needed
        if (n == 1) {
            *out = Ops::at(a, 0, 0);
        } else if (n == 2) {
            *out = Ops::at(a, 0, 0) * Ops::at(a, 1, 1) - Ops::at(a, 0, 1) * Ops::at(a, 1, 0);
        } else {
            *out = Ops::at(a, 0, 0) *
                       (Ops::at(a, 1, 1) * Ops::at(a, 2, 2) - Ops::at(a, 1, 2) * Ops::at(a, 2, 1)) -
                   Ops::at(a, 0, 1) *
                       (Ops::at(a, 1, 0) * Ops::at(a, 2, 2) - Ops::at(a, 1, 2) * Ops::at(a, 2, 0)) +
                   Ops::at(a, 0, 2) *
                       (Ops::at(a, 1, 0) * Ops::at(a, 2, 1) - Ops::at(a, 1, 1) * Ops::at(a, 2, 0));
        }
        return true;
    }
    Array& w = g_mwork[0];
    if (!copy(a, w)) {
        *err = kErrMemory;
        return false;
    }
    int sign = 1;
    const int pivots = forward_eliminate_t<Ops>(w, pivot_eps_t<Ops>(a), &sign);
    if (pivots < n) {
        *out = typename Ops::T{};  // Rank-deficient: det is exactly 0
    } else {
        typename Ops::T det = Ops::one();
        if (sign < 0) {
            det = -det;
        }
        for (int i = 0; i < n; ++i) {
            det = det * Ops::at(w, i, i);
        }
        *out = det;
    }
    w.clear();
    return true;
}

template <typename Ops>
bool inverse_t(const Array& a, Array& out, const char** err) {
    const int n = a.dim(0);
    Array& w = g_mwork[0];
    if (!copy(a, w)) {
        w.clear();
        *err = kErrMemory;
        return false;
    }
    fill_identity_t<Ops>(out, n);
    const calc_t eps = pivot_eps_t<Ops>(a);
    bool ok = true;
    // Gauss-Jordan: every row op on W is mirrored on `out`, which
    // starts as I and ends as A^-1.
    for (int col = 0; col < n; ++col) {
        int best = -1;
        calc_t best_abs = eps;
        for (int r = col; r < n; ++r) {
            const calc_t v = Ops::mag(Ops::at(w, r, col));
            if (v > best_abs) {
                best_abs = v;
                best = r;
            }
        }
        if (best < 0) {
            *err = kErrSingular;
            ok = false;
            break;
        }
        swap_rows_t<Ops>(w, best, col);
        swap_rows_t<Ops>(out, best, col);
        const typename Ops::T inv_piv = Ops::one() / Ops::at(w, col, col);
        scale_row_t<Ops>(w, col, inv_piv);
        scale_row_t<Ops>(out, col, inv_piv);
        for (int r = 0; r < n; ++r) {
            if (r == col) {
                continue;
            }
            const typename Ops::T v = Ops::at(w, r, col);
            if (Ops::mag(v) != 0) {
                add_scaled_row_t<Ops>(w, r, col, -v);
                add_scaled_row_t<Ops>(out, r, col, -v);
            }
        }
    }
    w.clear();
    return ok;
}

template <typename Ops>
void rref_t(Array& out, calc_t eps) {
    const int rows = out.dim(0);
    const int cols = out.dim(1);
    int pivots = 0;
    for (int col = 0; col < cols && pivots < rows; ++col) {
        int best = -1;
        calc_t best_abs = eps;
        for (int r = pivots; r < rows; ++r) {
            const calc_t v = Ops::mag(Ops::at(out, r, col));
            if (v > best_abs) {
                best_abs = v;
                best = r;
            }
        }
        if (best < 0) {
            continue;
        }
        swap_rows_t<Ops>(out, best, pivots);
        scale_row_t<Ops>(out, pivots, Ops::one() / Ops::at(out, pivots, col));
        for (int r = 0; r < rows; ++r) {
            if (r == pivots) {
                continue;
            }
            const typename Ops::T v = Ops::at(out, r, col);
            if (Ops::mag(v) != 0) {
                add_scaled_row_t<Ops>(out, r, pivots, -v);
            }
        }
        ++pivots;
    }
}

}  // namespace

bool copy(const Array& src, Array& dst) {
    if (&src == &dst) {
        return true;  // MatAns -> MatAns self-copy (4D.14)
    }
    if (!retype(dst, src.dtype()) || !dst.resize(src.dim(0), src.dim(1))) {
        return false;
    }
    const int n = src.size();
    if (src.dtype() == Dtype::kComplex) {
        for (int at = 0; at < n; at += kMaxRowElems) {
            const int m = n - at < kMaxRowElems ? n - at : kMaxRowElems;
            src.read_range_c(at, m, g_rowc.c);
            dst.write_range_c(at, m, g_rowc.c);
        }
        return true;
    }
    for (int at = 0; at < n; at += kMaxRowElems) {
        const int m = n - at < kMaxRowElems ? n - at : kMaxRowElems;
        src.read_range(at, m, g_rowc.d);
        dst.write_range(at, m, g_rowc.d);
    }
    return true;
}

bool make_complex(Array& m) {
    if (m.dtype() == Dtype::kComplex) {
        return true;
    }
    if (!m.is_matrix()) {
        return false;
    }
    g_staging.clear();
    if (!g_staging.set_dtype(Dtype::kComplex) || !g_staging.resize(m.dim(0), m.dim(1))) {
        g_staging.clear();
        return false;
    }
    const int n = m.size();
    for (int at = 0; at < n; at += kMaxRowElems) {
        const int cnt = n - at < kMaxRowElems ? n - at : kMaxRowElems;
        CplxOps::read(m, at, cnt, g_rowa);  // Promotes the real elements
        g_staging.write_range_c(at, cnt, g_rowa.c);
    }
    const bool ok = copy(g_staging, m);
    g_staging.clear();
    return ok;
}

namespace {

bool elementwise(const Array& a, const Array& b, Array& out, bool subtract, const char** err) {
    if (!valid(a) || !valid(b)) {
        *err = kErrNotMatrix;
        return false;
    }
    const int rows = a.dim(0);
    const int cols = a.dim(1);
    if (b.dim(0) != rows || b.dim(1) != cols) {
        *err = kErrDim;
        return false;
    }
    if (!retype(out, join(a, b)) || !out.resize(rows, cols)) {
        *err = kErrMemory;
        return false;
    }
    if (out.dtype() == Dtype::kComplex) {
        elementwise_t<CplxOps>(a, b, out, subtract);
    } else {
        elementwise_t<RealOps>(a, b, out, subtract);
    }
    return true;
}

}  // namespace

bool add(const Array& a, const Array& b, Array& out, const char** err) {
    return elementwise(a, b, out, false, err);
}

bool sub(const Array& a, const Array& b, Array& out, const char** err) {
    return elementwise(a, b, out, true, err);
}

bool mul(const Array& a, const Array& b, Array& out, const char** err) {
    if (!valid(a) || !valid(b)) {
        *err = kErrNotMatrix;
        return false;
    }
    const int rows = a.dim(0);
    const int inner = a.dim(1);
    const int cols = b.dim(1);
    if (b.dim(0) != inner) {
        *err = kErrDim;
        return false;
    }
    if (!dims_ok(rows, cols)) {
        *err = kErrTooLarge;
        return false;
    }
    if (!retype(out, join(a, b)) || !out.resize(rows, cols)) {
        *err = kErrMemory;
        return false;
    }
    if (out.dtype() == Dtype::kComplex) {
        mul_t<CplxOps>(a, b, out);
    } else {
        mul_t<RealOps>(a, b, out);
    }
    return true;
}

bool scalar_mul(const Array& a, calc_t k, Array& out, const char** err) {
    if (!valid(a)) {
        *err = kErrNotMatrix;
        return false;
    }
    if (a.dtype() == Dtype::kComplex) {
        return scalar_mul(a, Complex(k), out, err);
    }
    if (!retype(out, Dtype::kDouble) || !out.resize(a.dim(0), a.dim(1))) {
        *err = kErrMemory;
        return false;
    }
    const int n = a.size();
    for (int at = 0; at < n; at += kMaxRowElems) {
        const int m = n - at < kMaxRowElems ? n - at : kMaxRowElems;
        a.read_range(at, m, g_rowa.d);
        for (int i = 0; i < m; ++i) {
            g_rowa.d[i] *= k;
        }
        out.write_range(at, m, g_rowa.d);
    }
    return true;
}

bool scalar_mul(const Array& a, const Complex& k, Array& out, const char** err) {
    if (!valid(a)) {
        *err = kErrNotMatrix;
        return false;
    }
    if (a.dtype() == Dtype::kDouble && k.im == 0) {
        return scalar_mul(a, k.re, out, err);
    }
    if (!retype(out, Dtype::kComplex) || !out.resize(a.dim(0), a.dim(1))) {
        *err = kErrMemory;
        return false;
    }
    const int n = a.size();
    for (int at = 0; at < n; at += kMaxRowElems) {
        const int m = n - at < kMaxRowElems ? n - at : kMaxRowElems;
        CplxOps::read(a, at, m, g_rowa);
        for (int i = 0; i < m; ++i) {
            g_rowa.c[i] = g_rowa.c[i] * k;
        }
        out.write_range_c(at, m, g_rowa.c);
    }
    return true;
}

bool transpose(const Array& a, Array& out, const char** err) {
    if (!valid(a)) {
        *err = kErrNotMatrix;
        return false;
    }
    if (!retype(out, a.dtype()) || !out.resize(a.dim(1), a.dim(0))) {
        *err = kErrMemory;
        return false;
    }
    if (out.dtype() == Dtype::kComplex) {
        transpose_t<CplxOps>(a, out);
    } else {
        transpose_t<RealOps>(a, out);
    }
    return true;
}

bool identity(int n, Array& out, const char** err) {
    if (n < 1 || n > kMaxDim) {
        *err = kErrDim;
        return false;
    }
    if (!retype(out, Dtype::kDouble) || !out.resize(n, n)) {
        *err = kErrMemory;
        return false;
    }
    fill_identity_t<RealOps>(out, n);
    return true;
}

bool augment(const Array& a, const Array& b, Array& out, const char** err) {
    if (!valid(a) || !valid(b)) {
        *err = kErrNotMatrix;
        return false;
    }
    const int rows = a.dim(0);
    const int ca = a.dim(1);
    const int cb = b.dim(1);
    if (b.dim(0) != rows) {
        *err = kErrDim;
        return false;
    }
    if (!dims_ok(rows, ca + cb)) {
        *err = kErrTooLarge;
        return false;
    }
    if (!retype(out, join(a, b)) || !out.resize(rows, ca + cb)) {
        *err = kErrMemory;
        return false;
    }
    if (out.dtype() == Dtype::kComplex) {
        augment_t<CplxOps>(a, b, out);
    } else {
        augment_t<RealOps>(a, b, out);
    }
    return true;
}

bool reshape(const Array& a, int rows, int cols, Array& out, const char** err) {
    if (rows < 1 || cols < 1 || rows > kMaxDim || cols > kMaxDim) {
        *err = kErrDim;
        return false;
    }
    // An empty source (editor DIM creating a matrix) starts real.
    const Dtype t = (a.is_matrix() && a.size() > 0) ? a.dtype() : Dtype::kDouble;
    if (!retype(out, t) || !out.resize(rows, cols)) {
        *err = kErrMemory;
        return false;
    }
    if (out.dtype() == Dtype::kComplex) {
        reshape_t<CplxOps>(a, rows, cols, out);
    } else {
        reshape_t<RealOps>(a, rows, cols, out);
    }
    return true;
}

bool determinant(const Array& a, calc_t* out, const char** err) {
    if (!valid(a)) {
        *err = kErrNotMatrix;
        return false;
    }
    if (a.dtype() == Dtype::kComplex) {
        *err = kErrComplexMat;  // Real-only entry point (D37): never truncate
        return false;
    }
    if (a.dim(1) != a.dim(0)) {
        *err = kErrNotSquare;
        return false;
    }
    return det_t<RealOps>(a, out, err);
}

bool determinant(const Array& a, Complex* out, const char** err) {
    if (!valid(a)) {
        *err = kErrNotMatrix;
        return false;
    }
    if (a.dim(1) != a.dim(0)) {
        *err = kErrNotSquare;
        return false;
    }
    if (a.dtype() == Dtype::kComplex) {
        return det_t<CplxOps>(a, out, err);
    }
    calc_t d = 0;
    if (!det_t<RealOps>(a, &d, err)) {
        return false;
    }
    *out = Complex(d);
    return true;
}

bool inverse(const Array& a, Array& out, const char** err) {
    if (!valid(a)) {
        *err = kErrNotMatrix;
        return false;
    }
    const int n = a.dim(0);
    if (a.dim(1) != n) {
        *err = kErrNotSquare;
        return false;
    }
    if (!retype(out, a.dtype()) || !out.resize(n, n)) {
        *err = kErrMemory;
        return false;
    }
    return a.dtype() == Dtype::kComplex ? inverse_t<CplxOps>(a, out, err)
                                        : inverse_t<RealOps>(a, out, err);
}

bool ref(const Array& a, Array& out, const char** err) {
    if (!valid(a)) {
        *err = kErrNotMatrix;
        return false;
    }
    if (!copy(a, out)) {
        *err = kErrMemory;
        return false;
    }
    if (out.dtype() == Dtype::kComplex) {
        forward_eliminate_t<CplxOps>(out, pivot_eps_t<CplxOps>(a), nullptr);
    } else {
        forward_eliminate_t<RealOps>(out, pivot_eps_t<RealOps>(a), nullptr);
    }
    return true;
}

bool rref(const Array& a, Array& out, const char** err) {
    if (!valid(a)) {
        *err = kErrNotMatrix;
        return false;
    }
    if (!copy(a, out)) {
        *err = kErrMemory;
        return false;
    }
    if (out.dtype() == Dtype::kComplex) {
        rref_t<CplxOps>(out, pivot_eps_t<CplxOps>(a));
    } else {
        rref_t<RealOps>(out, pivot_eps_t<RealOps>(a));
    }
    return true;
}

bool rank(const Array& a, int* out, const char** err) {
    if (!valid(a)) {
        *err = kErrNotMatrix;
        return false;
    }
    Array& w = g_mwork[0];
    if (!copy(a, w)) {
        *err = kErrMemory;
        return false;
    }
    if (w.dtype() == Dtype::kComplex) {
        *out = forward_eliminate_t<CplxOps>(w, pivot_eps_t<CplxOps>(a), nullptr);
    } else {
        *out = forward_eliminate_t<RealOps>(w, pivot_eps_t<RealOps>(a), nullptr);
    }
    w.clear();
    return true;
}

bool power(const Array& a, int p, Array& out, const char** err) {
    if (!valid(a)) {
        *err = kErrNotMatrix;
        return false;
    }
    if (a.dim(0) != a.dim(1)) {
        *err = kErrNotSquare;
        return false;
    }
    if (p == -1) {
        return inverse(a, out, err);
    }
    if (p < -1 || p > kMaxPower) {
        *err = "Exponent out of range";
        return false;
    }
    if (p == 0) {
        return identity(a.dim(0), out, err);
    }
    if (p == 1) {
        if (!copy(a, out)) {
            *err = kErrMemory;
            return false;
        }
        return true;
    }
    // out = a^p by repeated multiplication through g_mwork[1] (mul
    // requires distinct output; g_mwork[0] stays free for inverse's
    // callers). p <= 100 keeps this simple over square-and-multiply.
    Array& t = g_mwork[1];
    if (!copy(a, out)) {
        *err = kErrMemory;
        return false;
    }
    bool ok = true;
    for (int i = 1; i < p; ++i) {
        if (!mul(out, a, t, err) || !copy(t, out)) {
            ok = false;
            break;
        }
    }
    t.clear();
    return ok;
}

bool norm_f(const Array& a, calc_t* out, const char** err) {
    if (!valid(a)) {
        *err = kErrNotMatrix;
        return false;
    }
    double acc = 0;
    const int n = a.size();
    if (a.dtype() == Dtype::kComplex) {
        for (int at = 0; at < n; at += kMaxRowElems) {
            const int m = n - at < kMaxRowElems ? n - at : kMaxRowElems;
            a.read_range_c(at, m, g_rowa.c);
            for (int i = 0; i < m; ++i) {
                acc += g_rowa.c[i].re * g_rowa.c[i].re + g_rowa.c[i].im * g_rowa.c[i].im;
            }
        }
    } else {
        for (int at = 0; at < n; at += kMaxRowElems) {
            const int m = n - at < kMaxRowElems ? n - at : kMaxRowElems;
            a.read_range(at, m, g_rowa.d);
            for (int i = 0; i < m; ++i) {
                acc += g_rowa.d[i] * g_rowa.d[i];
            }
        }
    }
    *out = std::sqrt(acc);
    return true;
}

namespace {

// Shared Hessenberg + shifted-QR core (D28/D30): finds the full
// spectrum as Complex (real results carry im == 0), never erroring on
// a complex-conjugate pair itself — that's now a legitimate spectrum
// entry rather than a domain error. `eig` must hold at least
// kMaxEigen entries. Input stays real-only (D37): a complex-valued
// matrix errors rather than reading truncated real parts.
bool eigen_core(const Array& a, Complex* eig, int* out_count, const char** err) {
    if (!valid(a)) {
        *err = kErrNotMatrix;
        return false;
    }
    if (a.dtype() == Dtype::kComplex) {
        *err = kErrComplexMat;
        return false;
    }
    const int n = a.dim(0);
    if (a.dim(1) != n) {
        *err = kErrNotSquare;
        return false;
    }
    if (n > kMaxEigen) {
        *err = "Eigen limit is 10x10";
        return false;
    }
    // n <= 10: the whole problem fits an SRAM scratch matrix.
    calc_t h[kMaxEigen][kMaxEigen];
    for (int r = 0; r < n; ++r) {
        a.read_range(r * n, n, g_rowa.d);
        for (int c = 0; c < n; ++c) {
            h[r][c] = g_rowa.d[c];
        }
    }

    calc_t norm = 0;
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            norm += std::fabs(h[r][c]);
        }
    }
    const calc_t eps = 1e-14 * (norm > 1.0 ? norm : 1.0);

    // Reduce to upper Hessenberg form with Givens rotations (numerically
    // adequate at n <= 10; Householder would be overkill here).
    for (int col = 0; col < n - 2; ++col) {
        for (int r = col + 2; r < n; ++r) {
            if (std::fabs(h[r][col]) <= eps) {
                h[r][col] = 0;
                continue;
            }
            const calc_t x = h[col + 1][col];
            const calc_t y = h[r][col];
            const calc_t rad = std::hypot(x, y);
            const calc_t cs = x / rad;
            const calc_t sn = y / rad;
            for (int c = 0; c < n; ++c) {  // Row rotation
                const calc_t t1 = h[col + 1][c];
                const calc_t t2 = h[r][c];
                h[col + 1][c] = cs * t1 + sn * t2;
                h[r][c] = -sn * t1 + cs * t2;
            }
            for (int rr = 0; rr < n; ++rr) {  // Column rotation (similarity)
                const calc_t t1 = h[rr][col + 1];
                const calc_t t2 = h[rr][r];
                h[rr][col + 1] = cs * t1 + sn * t2;
                h[rr][r] = -sn * t1 + cs * t2;
            }
        }
    }

    // Shifted QR iteration with deflation from the bottom.
    int found = 0;
    int m = n - 1;  // Active block is h[0..m][0..m]
    int iters = 0;
    const int max_iters = 30 * n + 30;
    while (m >= 0) {
        if (m == 0) {
            eig[found++] = Complex(h[0][0]);
            break;
        }
        if (std::fabs(h[m][m - 1]) <= eps) {
            eig[found++] = Complex(h[m][m]);
            --m;
            continue;
        }
        // Trailing 2x2 block: deflatable when isolated or complex.
        const calc_t p2 = h[m - 1][m - 1];
        const calc_t q2 = h[m - 1][m];
        const calc_t r2 = h[m][m - 1];
        const calc_t s2 = h[m][m];
        const calc_t tr = p2 + s2;
        const calc_t disc = (p2 - s2) * (p2 - s2) + 4.0 * q2 * r2;
        const bool isolated = m == 1 || std::fabs(h[m - 1][m - 2]) <= eps;
        if (isolated) {
            if (disc < 0) {
                // Complex-conjugate pair (Phase 4C, D30 — was an error
                // pre-4C/D28). +i entry first, matching the display sort
                // below.
                const calc_t sq = std::sqrt(-disc);
                eig[found++] = Complex(0.5 * tr, 0.5 * sq);
                eig[found++] = Complex(0.5 * tr, -0.5 * sq);
            } else {
                const calc_t sq = std::sqrt(disc);
                eig[found++] = Complex(0.5 * (tr + sq));
                eig[found++] = Complex(0.5 * (tr - sq));
            }
            m -= 2;
            continue;
        }
        if (++iters > max_iters) {
            *err = "Eigenvalues did not converge";
            return false;
        }
        // Wilkinson shift: the eigenvalue of the trailing 2x2 nearest
        // h[m][m] (its real part when the pair is complex).
        calc_t shift = 0;
        if (disc >= 0) {
            const calc_t sq = std::sqrt(disc);
            const calc_t e1 = 0.5 * (tr + sq);
            const calc_t e2 = 0.5 * (tr - sq);
            shift = std::fabs(e1 - s2) < std::fabs(e2 - s2) ? e1 : e2;
        } else {
            shift = 0.5 * tr;
        }
        for (int i = 0; i <= m; ++i) {
            h[i][i] -= shift;
        }
        // One explicit QR sweep on the Hessenberg block via Givens:
        // Q^T H, then (Q^T H) Q, using stored rotations.
        calc_t cs_arr[kMaxEigen];
        calc_t sn_arr[kMaxEigen];
        for (int i = 0; i < m; ++i) {
            const calc_t x = h[i][i];
            const calc_t y = h[i + 1][i];
            const calc_t rad = std::hypot(x, y);
            if (rad <= eps) {
                cs_arr[i] = 1;
                sn_arr[i] = 0;
                continue;
            }
            cs_arr[i] = x / rad;
            sn_arr[i] = y / rad;
            for (int c = i; c <= m; ++c) {
                const calc_t t1 = h[i][c];
                const calc_t t2 = h[i + 1][c];
                h[i][c] = cs_arr[i] * t1 + sn_arr[i] * t2;
                h[i + 1][c] = -sn_arr[i] * t1 + cs_arr[i] * t2;
            }
        }
        for (int i = 0; i < m; ++i) {
            for (int r = 0; r <= (i + 1 < m ? i + 1 : m); ++r) {
                const calc_t t1 = h[r][i];
                const calc_t t2 = h[r][i + 1];
                h[r][i] = cs_arr[i] * t1 + sn_arr[i] * t2;
                h[r][i + 1] = -sn_arr[i] * t1 + cs_arr[i] * t2;
            }
        }
        for (int i = 0; i <= m; ++i) {
            h[i][i] += shift;
        }
    }

    // Descending by real part; a conjugate pair's +i entry precedes -i.
    for (int i = 1; i < found; ++i) {
        const Complex v = eig[i];
        int j = i;
        while (j > 0 && (eig[j - 1].re < v.re || (eig[j - 1].re == v.re && eig[j - 1].im < v.im))) {
            eig[j] = eig[j - 1];
            --j;
        }
        eig[j] = v;
    }
    *out_count = found;
    return true;
}

}  // namespace

bool eigenvalues(const Array& a, Array& out, const char** err) {
    Complex eig[kMaxEigen];
    int found = 0;
    if (!eigen_core(a, eig, &found, err)) {
        return false;
    }
    for (int i = 0; i < found; ++i) {
        if (!eig[i].is_real()) {
            *err = "Complex eigenvalues";
            return false;
        }
    }
    calc_t real_eig[kMaxEigen];
    for (int i = 0; i < found; ++i) {
        real_eig[i] = eig[i].re;
    }
    if (!retype(out, Dtype::kDouble) || !out.resize(found)) {
        *err = kErrMemory;
        return false;
    }
    out.write_range(0, found, real_eig);
    return true;
}

bool eigenvalues_complex(const Array& a, Complex* out, int* count, const char** err) {
    return eigen_core(a, out, count, err);
}

bool eigenvectors(const Array& a, Array& out, const char** err) {
    Complex eig[kMaxEigen];
    int found = 0;
    if (!eigen_core(a, eig, &found, err)) {
        return false;
    }
    const int n = found;
    for (int i = 0; i < n; ++i) {
        if (!eig[i].is_real()) {
            *err = "Complex eigenvalues";
            return false;
        }
    }
    // Distinct eigenvalues only (D38/P4-13): a repeated eigenvalue has
    // either a whole eigenspace (no unique choice) or none (defective).
    for (int i = 1; i < n; ++i) {
        if (std::fabs(eig[i].re - eig[i - 1].re) <=
            1e-8 * (std::fabs(eig[i].re) > 1.0 ? std::fabs(eig[i].re) : 1.0)) {
            *err = "No unique eigenvector";
            return false;
        }
    }
    if (!retype(out, Dtype::kDouble) || !out.resize(n, n)) {
        *err = kErrMemory;
        return false;
    }
    // Working copies: B = A - lambda I in g_mwork[1], its rref in
    // g_mwork[0] (both free here — eigen_core used only SRAM scratch).
    Array& b = g_mwork[1];
    Array& r = g_mwork[0];
    for (int k = 0; k < n; ++k) {
        const calc_t lambda = eig[k].re;
        bool ok = copy(a, b);
        for (int i = 0; ok && i < n; ++i) {
            b.set(i, i, b.get(i, i) - lambda);
        }
        ok = ok && copy(b, r);
        if (!ok) {
            b.clear();
            r.clear();
            *err = kErrMemory;
            return false;
        }
        // Looser epsilon than rref()'s 1e-12-relative: lambda carries
        // QR error, so (A - lambda I) is only near-singular.
        rref_t<RealOps>(r, 1e4 * pivot_eps_t<RealOps>(b));
        // Pivot columns: each nonzero row's leading entry. Exactly one
        // free column = a 1-D nullspace.
        bool is_pivot[kMaxEigen] = {};
        int rank = 0;
        for (int row = 0; row < n; ++row) {
            for (int col = 0; col < n; ++col) {
                if (std::fabs(r.get(row, col)) > 0.5) {
                    is_pivot[col] = true;
                    ++rank;
                    break;
                }
            }
        }
        if (rank != n - 1) {
            b.clear();
            r.clear();
            *err = "No unique eigenvector";
            return false;
        }
        int free_col = -1;
        for (int col = 0; col < n; ++col) {
            if (!is_pivot[col]) {
                free_col = col;
                break;
            }
        }
        // v[free] = 1; each pivot row gives v[pivot_col] = -R[row][free].
        calc_t v[kMaxEigen] = {};
        v[free_col] = 1.0;
        for (int row = 0; row < n; ++row) {
            for (int col = 0; col < n; ++col) {
                if (std::fabs(r.get(row, col)) > 0.5) {  // Leading 1
                    v[col] = -r.get(row, free_col);
                    break;
                }
            }
        }
        // Normalize to unit length; sign so the largest-magnitude
        // component (first on ties) is positive — deterministic output.
        calc_t norm = 0;
        for (int i = 0; i < n; ++i) {
            norm += v[i] * v[i];
        }
        norm = std::sqrt(norm);
        int big = 0;
        for (int i = 1; i < n; ++i) {
            if (std::fabs(v[i]) > std::fabs(v[big])) {
                big = i;
            }
        }
        const calc_t scale = (v[big] < 0 ? -1.0 : 1.0) / norm;
        for (int i = 0; i < n; ++i) {
            out.set(i, k, v[i] * scale);
        }
    }
    b.clear();
    r.clear();
    return true;
}

}  // namespace math::matops

namespace math {

MatrixStore& matrices() {
    static MatrixStore instance;
    return instance;
}

}  // namespace math
