#include "math/matrix.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace math::matops {

namespace {

// Streaming row buffers. Never more than three rows are in flight at
// once (e.g. mul: one A row, one B row, one accumulator row).
calc_t g_rowa[kMaxRowElems];
calc_t g_rowb[kMaxRowElems];
calc_t g_rowc[kMaxRowElems];

// Working copies for the elimination algorithms and power().
Array g_mwork[2];

const char* const kErrNotMatrix = "Not a matrix";
const char* const kErrDim = "Dim mismatch";
const char* const kErrNotSquare = "Not square";
const char* const kErrTooLarge = "Matrix too large";
const char* const kErrMemory = "Out of matrix memory";
const char* const kErrSingular = "Singular matrix";

bool valid(const Array& a) {
    return a.is_matrix() && a.dim(0) >= 1 && a.dim(1) >= 1;
}

bool dims_ok(int rows, int cols) {
    return rows >= 1 && cols >= 1 && rows <= kMaxRowElems && cols <= kMaxRowElems &&
           rows * cols <= Array::kMaxElements;
}

// Pivot tolerance, relative to the matrix's largest magnitude (so
// scaling A by 1e-6 doesn't turn it "singular").
calc_t pivot_eps(const Array& a) {
    calc_t maxabs = 0;
    const int n = a.size();
    for (int at = 0; at < n; at += kMaxRowElems) {
        const int m = n - at < kMaxRowElems ? n - at : kMaxRowElems;
        a.read_range(at, m, g_rowa);
        for (int i = 0; i < m; ++i) {
            maxabs = std::max(std::fabs(g_rowa[i]), maxabs);
        }
    }
    return 1e-12 * (maxabs > 1.0 ? maxabs : 1.0);
}

void read_row(const Array& a, int r, calc_t* buf) {
    a.read_range(r * a.dim(1), a.dim(1), buf);
}

void write_row(Array& a, int r, const calc_t* buf) {
    a.write_range(r * a.dim(1), a.dim(1), buf);
}

void swap_rows(Array& a, int r1, int r2) {
    if (r1 == r2) {
        return;
    }
    read_row(a, r1, g_rowa);
    read_row(a, r2, g_rowb);
    write_row(a, r1, g_rowb);
    write_row(a, r2, g_rowa);
}

// a[dst,:] += factor * a[src,:]
void add_scaled_row(Array& a, int dst, int src, calc_t factor) {
    const int cols = a.dim(1);
    read_row(a, dst, g_rowa);
    read_row(a, src, g_rowb);
    for (int c = 0; c < cols; ++c) {
        g_rowa[c] += factor * g_rowb[c];
    }
    write_row(a, dst, g_rowa);
}

void scale_row(Array& a, int r, calc_t factor) {
    const int cols = a.dim(1);
    read_row(a, r, g_rowa);
    for (int c = 0; c < cols; ++c) {
        g_rowa[c] *= factor;
    }
    write_row(a, r, g_rowa);
}

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
    if (!out.resize(rows, cols)) {
        *err = kErrMemory;
        return false;
    }
    for (int r = 0; r < rows; ++r) {
        read_row(a, r, g_rowa);
        read_row(b, r, g_rowb);
        for (int c = 0; c < cols; ++c) {
            g_rowa[c] = subtract ? g_rowa[c] - g_rowb[c] : g_rowa[c] + g_rowb[c];
        }
        write_row(out, r, g_rowa);
    }
    return true;
}

// Forward elimination to (unreduced) row echelon form, in place.
// Returns the pivot count; *sign flips per row swap (for det).
int forward_eliminate(Array& w, calc_t eps, int* sign) {
    const int rows = w.dim(0);
    const int cols = w.dim(1);
    int pivots = 0;
    for (int col = 0; col < cols && pivots < rows; ++col) {
        int best = -1;
        calc_t best_abs = eps;
        for (int r = pivots; r < rows; ++r) {
            const calc_t v = std::fabs(w.get(r, col));
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
        swap_rows(w, best, pivots);
        const calc_t piv = w.get(pivots, col);
        for (int r = pivots + 1; r < rows; ++r) {
            const calc_t v = w.get(r, col);
            if (v != 0) {
                add_scaled_row(w, r, pivots, -v / piv);
            }
        }
        ++pivots;
    }
    return pivots;
}

}  // namespace

bool copy(const Array& src, Array& dst) {
    if (!dst.resize(src.dim(0), src.dim(1))) {
        return false;
    }
    const int n = src.size();
    for (int at = 0; at < n; at += kMaxRowElems) {
        const int m = n - at < kMaxRowElems ? n - at : kMaxRowElems;
        src.read_range(at, m, g_rowc);
        dst.write_range(at, m, g_rowc);
    }
    return true;
}

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
    if (!out.resize(rows, cols)) {
        *err = kErrMemory;
        return false;
    }
    // C[i,:] = sum_k A[i,k] * B[k,:] — row-major streaming friendly
    // for the PSRAM tier (no strided column reads).
    for (int i = 0; i < rows; ++i) {
        read_row(a, i, g_rowa);
        for (int c = 0; c < cols; ++c) {
            g_rowc[c] = 0;
        }
        for (int k = 0; k < inner; ++k) {
            if (g_rowa[k] == 0) {
                continue;
            }
            read_row(b, k, g_rowb);
            for (int c = 0; c < cols; ++c) {
                g_rowc[c] += g_rowa[k] * g_rowb[c];
            }
        }
        write_row(out, i, g_rowc);
    }
    return true;
}

bool scalar_mul(const Array& a, calc_t k, Array& out, const char** err) {
    if (!valid(a)) {
        *err = kErrNotMatrix;
        return false;
    }
    if (!out.resize(a.dim(0), a.dim(1))) {
        *err = kErrMemory;
        return false;
    }
    const int n = a.size();
    for (int at = 0; at < n; at += kMaxRowElems) {
        const int m = n - at < kMaxRowElems ? n - at : kMaxRowElems;
        a.read_range(at, m, g_rowa);
        for (int i = 0; i < m; ++i) {
            g_rowa[i] *= k;
        }
        out.write_range(at, m, g_rowa);
    }
    return true;
}

bool transpose(const Array& a, Array& out, const char** err) {
    if (!valid(a)) {
        *err = kErrNotMatrix;
        return false;
    }
    const int rows = a.dim(0);
    const int cols = a.dim(1);
    if (!out.resize(cols, rows)) {
        *err = kErrMemory;
        return false;
    }
    // Read a source row, scatter it as a destination column. The
    // scatter is per-element, but each element is touched exactly once.
    for (int r = 0; r < rows; ++r) {
        read_row(a, r, g_rowa);
        for (int c = 0; c < cols; ++c) {
            out.set(c, r, g_rowa[c]);
        }
    }
    return true;
}

bool identity(int n, Array& out, const char** err) {
    if (n < 1 || n > kMaxDim) {
        *err = kErrDim;
        return false;
    }
    if (!out.resize(n, n)) {
        *err = kErrMemory;
        return false;
    }
    for (int c = 0; c < n; ++c) {
        g_rowa[c] = 0;
    }
    for (int r = 0; r < n; ++r) {
        if (r > 0) {
            g_rowa[r - 1] = 0;
        }
        g_rowa[r] = 1;
        write_row(out, r, g_rowa);
    }
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
    if (!out.resize(rows, ca + cb)) {
        *err = kErrMemory;
        return false;
    }
    for (int r = 0; r < rows; ++r) {
        read_row(a, r, g_rowc);
        b.read_range(r * cb, cb, g_rowc + ca);
        write_row(out, r, g_rowc);
    }
    return true;
}

bool reshape(const Array& a, int rows, int cols, Array& out, const char** err) {
    if (rows < 1 || cols < 1 || rows > kMaxDim || cols > kMaxDim) {
        *err = kErrDim;
        return false;
    }
    if (!out.resize(rows, cols)) {
        *err = kErrMemory;
        return false;
    }
    out.fill(0);
    if (a.size() == 0 || !a.is_matrix()) {
        return true;
    }
    const int copy_rows = rows < a.dim(0) ? rows : a.dim(0);
    const int copy_cols = cols < a.dim(1) ? cols : a.dim(1);
    for (int r = 0; r < copy_rows; ++r) {
        a.read_range(r * a.dim(1), copy_cols, g_rowa);
        out.write_range(r * cols, copy_cols, g_rowa);
    }
    return true;
}

bool determinant(const Array& a, calc_t* out, const char** err) {
    if (!valid(a)) {
        *err = kErrNotMatrix;
        return false;
    }
    const int n = a.dim(0);
    if (a.dim(1) != n) {
        *err = kErrNotSquare;
        return false;
    }
    if (n <= 3) {  // Direct expansion — no working copy needed
        if (n == 1) {
            *out = a.get(0, 0);
        } else if (n == 2) {
            *out = a.get(0, 0) * a.get(1, 1) - a.get(0, 1) * a.get(1, 0);
        } else {
            *out = a.get(0, 0) * (a.get(1, 1) * a.get(2, 2) - a.get(1, 2) * a.get(2, 1)) -
                   a.get(0, 1) * (a.get(1, 0) * a.get(2, 2) - a.get(1, 2) * a.get(2, 0)) +
                   a.get(0, 2) * (a.get(1, 0) * a.get(2, 1) - a.get(1, 1) * a.get(2, 0));
        }
        return true;
    }
    Array& w = g_mwork[0];
    if (!copy(a, w)) {
        *err = kErrMemory;
        return false;
    }
    int sign = 1;
    const int pivots = forward_eliminate(w, pivot_eps(a), &sign);
    if (pivots < n) {
        *out = 0;  // Rank-deficient: det is exactly 0
    } else {
        calc_t det = sign;
        for (int i = 0; i < n; ++i) {
            det *= w.get(i, i);
        }
        *out = det;
    }
    w.clear();
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
    Array& w = g_mwork[0];
    if (!copy(a, w) || !identity(n, out, err)) {
        w.clear();
        *err = kErrMemory;
        return false;
    }
    const calc_t eps = pivot_eps(a);
    bool ok = true;
    // Gauss-Jordan: every row op on W is mirrored on `out`, which
    // starts as I and ends as A^-1.
    for (int col = 0; col < n; ++col) {
        int best = -1;
        calc_t best_abs = eps;
        for (int r = col; r < n; ++r) {
            const calc_t v = std::fabs(w.get(r, col));
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
        swap_rows(w, best, col);
        swap_rows(out, best, col);
        const calc_t inv_piv = 1.0 / w.get(col, col);
        scale_row(w, col, inv_piv);
        scale_row(out, col, inv_piv);
        for (int r = 0; r < n; ++r) {
            if (r == col) {
                continue;
            }
            const calc_t v = w.get(r, col);
            if (v != 0) {
                add_scaled_row(w, r, col, -v);
                add_scaled_row(out, r, col, -v);
            }
        }
    }
    w.clear();
    return ok;
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
    forward_eliminate(out, pivot_eps(a), nullptr);
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
    const int rows = out.dim(0);
    const int cols = out.dim(1);
    const calc_t eps = pivot_eps(a);
    int pivots = 0;
    for (int col = 0; col < cols && pivots < rows; ++col) {
        int best = -1;
        calc_t best_abs = eps;
        for (int r = pivots; r < rows; ++r) {
            const calc_t v = std::fabs(out.get(r, col));
            if (v > best_abs) {
                best_abs = v;
                best = r;
            }
        }
        if (best < 0) {
            continue;
        }
        swap_rows(out, best, pivots);
        scale_row(out, pivots, 1.0 / out.get(pivots, col));
        for (int r = 0; r < rows; ++r) {
            if (r == pivots) {
                continue;
            }
            const calc_t v = out.get(r, col);
            if (v != 0) {
                add_scaled_row(out, r, pivots, -v);
            }
        }
        ++pivots;
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
    *out = forward_eliminate(w, pivot_eps(a), nullptr);
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

namespace {

// Shared Hessenberg + shifted-QR core (D28/D30): finds the full
// spectrum as Complex (real results carry im == 0), never erroring on
// a complex-conjugate pair itself — that's now a legitimate spectrum
// entry rather than a domain error. `eig` must hold at least
// kMaxEigen entries.
bool eigen_core(const Array& a, Complex* eig, int* out_count, const char** err) {
    if (!valid(a)) {
        *err = kErrNotMatrix;
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
        read_row(a, r, g_rowa);
        for (int c = 0; c < n; ++c) {
            h[r][c] = g_rowa[c];
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
    if (!out.resize(found)) {
        *err = kErrMemory;
        return false;
    }
    out.write_range(0, found, real_eig);
    return true;
}

bool eigenvalues_complex(const Array& a, Complex* out, int* count, const char** err) {
    return eigen_core(a, out, count, err);
}

}  // namespace math::matops

namespace math {

MatrixStore& matrices() {
    static MatrixStore instance;
    return instance;
}

}  // namespace math
