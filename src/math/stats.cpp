#include "math/stats.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace math::stats {

namespace {

constexpr int kChunk = 256;
constexpr calc_t kPi = 3.14159265358979323846;

// Streaming chunk buffers (single-core application code, no
// reentrancy — same pattern as list_ops).
calc_t g_bx[kChunk];
calc_t g_by[kChunk];
calc_t g_bf[kChunk];

constexpr const char* kErrEmpty = "Empty list";
constexpr const char* kErrComplex = "Non-real list";
constexpr const char* kErrLen = "List length mismatch";
constexpr const char* kErrNonFinite = "Non-finite data";
constexpr const char* kErrFreq = "Freq must be int >= 0";
constexpr const char* kErrFreqBig = "Freq total too large";
constexpr const char* kErrData = "Not enough data";
constexpr const char* kErrDomain = "Domain error";
constexpr const char* kErrSingular = "Singular matrix";

// ---- Streaming pass helpers -------------------------------------

template <typename F>
void for_each_x(const Array& xs, const F& f) {
    const int n = xs.size();
    for (int at = 0; at < n; at += kChunk) {
        const int m = n - at < kChunk ? n - at : kChunk;
        xs.read_range(at, m, g_bx);
        for (int i = 0; i < m; ++i) {
            f(g_bx[i]);
        }
    }
}

template <typename F>
void for_each_xy(const Array& xs, const Array& ys, const F& f) {
    const int n = xs.size();
    for (int at = 0; at < n; at += kChunk) {
        const int m = n - at < kChunk ? n - at : kChunk;
        xs.read_range(at, m, g_bx);
        ys.read_range(at, m, g_by);
        for (int i = 0; i < m; ++i) {
            f(g_bx[i], g_by[i]);
        }
    }
}

// ---- Rank selection over the ordered double bit space -----------
//
// key_of is a monotone bijection from finite doubles to uint64, so the
// k-th smallest element can be found by binary search on the key space
// with one streaming counting pass per bit — no sort, no temp copy,
// exact (the search converges onto an actual data value's key). All
// requested ranks advance together, so a batch costs <= 64 passes
// total, not per rank.

uint64_t key_of(calc_t v) {
    uint64_t b = 0;
    std::memcpy(&b, &v, sizeof(b));
    return (b & 0x8000000000000000ull) != 0 ? ~b : (b | 0x8000000000000000ull);
}

calc_t val_of(uint64_t k) {
    uint64_t b = (k & 0x8000000000000000ull) != 0 ? (k & 0x7FFFFFFFFFFFFFFFull) : ~k;
    calc_t v = 0;
    std::memcpy(&v, &b, sizeof(v));
    return v;
}

constexpr int kMaxRanks = 12;

// out[j] := the ks[j]-th smallest (1-based) value in vals, where
// element i carries weight round(freq[i]) (1 when freq == nullptr)
// and participates only when filter == nullptr or filter[i] is inside
// [flo, fhi]. Callers guarantee finite data and 1 <= ks[j] <= total
// weight; out-of-range ranks clamp to the extremes.
void select_ranks(const Array& vals, const Array* freq, const Array* filter, calc_t flo, calc_t fhi,
                  const long long* ks, int count, calc_t* out) {
    const int n = vals.size();
    uint64_t kmin = ~0ull;
    uint64_t kmax = 0;
    bool any = false;
    for (int at = 0; at < n; at += kChunk) {
        const int m = n - at < kChunk ? n - at : kChunk;
        vals.read_range(at, m, g_bx);
        if (freq != nullptr) {
            freq->read_range(at, m, g_bf);
        }
        if (filter != nullptr) {
            filter->read_range(at, m, g_by);
        }
        for (int i = 0; i < m; ++i) {
            if (filter != nullptr && (g_by[i] < flo || g_by[i] > fhi)) {
                continue;
            }
            if (freq != nullptr && std::llround(g_bf[i]) <= 0) {
                continue;
            }
            const uint64_t k = key_of(g_bx[i]);
            kmin = k < kmin ? k : kmin;
            kmax = k > kmax ? k : kmax;
            any = true;
        }
    }
    if (!any) {
        for (int j = 0; j < count; ++j) {
            out[j] = NAN;
        }
        return;
    }

    uint64_t lo[kMaxRanks];
    uint64_t hi[kMaxRanks];
    for (int j = 0; j < count; ++j) {
        lo[j] = kmin;
        hi[j] = kmax;
    }
    while (true) {
        int idx[kMaxRanks];
        uint64_t piv[kMaxRanks];
        long long wle[kMaxRanks] = {};
        int na = 0;
        for (int j = 0; j < count; ++j) {
            if (lo[j] < hi[j]) {
                idx[na] = j;
                piv[na] = lo[j] + (hi[j] - lo[j]) / 2;
                ++na;
            }
        }
        if (na == 0) {
            break;
        }
        for (int at = 0; at < n; at += kChunk) {
            const int m = n - at < kChunk ? n - at : kChunk;
            vals.read_range(at, m, g_bx);
            if (freq != nullptr) {
                freq->read_range(at, m, g_bf);
            }
            if (filter != nullptr) {
                filter->read_range(at, m, g_by);
            }
            for (int i = 0; i < m; ++i) {
                if (filter != nullptr && (g_by[i] < flo || g_by[i] > fhi)) {
                    continue;
                }
                const long long w = freq != nullptr ? std::llround(g_bf[i]) : 1;
                if (w <= 0) {
                    continue;
                }
                const uint64_t k = key_of(g_bx[i]);
                for (int t = 0; t < na; ++t) {
                    if (k <= piv[t]) {
                        wle[t] += w;
                    }
                }
            }
        }
        for (int t = 0; t < na; ++t) {
            const int j = idx[t];
            if (wle[t] >= ks[j]) {
                hi[j] = piv[t];
            } else {
                lo[j] = piv[t] + 1;
            }
        }
    }
    for (int j = 0; j < count; ++j) {
        out[j] = val_of(lo[j]);
    }
}

// TI quartile rule: the median of positions [base+1, base+h] in the
// expanded (weight-repeated) sorted order — two ranks whose values
// average (equal ranks when h is odd).
struct MedRanks {
    long long r1 = 0;
    long long r2 = 0;
};

MedRanks med_ranks(long long base, long long h) {
    if (h % 2 == 1) {
        return {base + (h + 1) / 2, base + (h + 1) / 2};
    }
    return {base + h / 2, base + h / 2 + 1};
}

// ---- 1-var / 2-var ----------------------------------------------

OneVarStats one_var_impl(const Array& data, const Array* freq) {
    OneVarStats s;
    // Real-only (D37): variance/ordering aren't defined for the
    // complex lists 4D.24 added — error, never truncate.
    if (data.dtype() != Dtype::kDouble || (freq != nullptr && freq->dtype() != Dtype::kDouble)) {
        s.error = kErrComplex;
        return s;
    }
    const int n = data.size();
    if (n == 0) {
        s.error = kErrEmpty;
        return s;
    }
    if (freq != nullptr && freq->size() != n) {
        s.error = kErrLen;
        return s;
    }

    long long w_total = 0;
    calc_t sum = 0;
    calc_t sum_sq = 0;
    calc_t mn = HUGE_VAL;
    calc_t mx = -HUGE_VAL;
    const char* err = nullptr;
    for (int at = 0; at < n && err == nullptr; at += kChunk) {
        const int m = n - at < kChunk ? n - at : kChunk;
        data.read_range(at, m, g_bx);
        if (freq != nullptr) {
            freq->read_range(at, m, g_bf);
        }
        for (int i = 0; i < m; ++i) {
            const calc_t x = g_bx[i];
            if (!std::isfinite(x)) {
                err = kErrNonFinite;
                break;
            }
            long long w = 1;
            if (freq != nullptr) {
                const calc_t f = g_bf[i];
                if (!std::isfinite(f) || f < 0 || f != std::floor(f)) {
                    err = kErrFreq;
                    break;
                }
                w = std::llround(f);
            }
            if (w == 0) {
                continue;  // freq 0 excludes the element entirely
            }
            w_total += w;
            sum += static_cast<calc_t>(w) * x;
            sum_sq += static_cast<calc_t>(w) * x * x;
            mn = x < mn ? x : mn;
            mx = x > mx ? x : mx;
        }
    }
    if (err != nullptr) {
        s.error = err;
        return s;
    }
    if (w_total == 0) {
        s.error = kErrEmpty;
        return s;
    }
    if (w_total > 1000000000LL) {
        s.error = kErrFreqBig;
        return s;
    }

    const auto w = static_cast<calc_t>(w_total);
    s.n = static_cast<int>(w_total);
    s.sum = sum;
    s.sum_sq = sum_sq;
    s.mean = sum / w;
    const calc_t ss = sum_sq - w * s.mean * s.mean;
    s.pop_stddev = std::sqrt(ss > 0 ? ss / w : 0);
    s.sample_stddev = w_total >= 2 ? std::sqrt(ss > 0 ? ss / (w - 1) : 0) : NAN;
    s.min_val = mn;
    s.max_val = mx;

    const long long h = w_total / 2;
    const MedRanks med = med_ranks(0, w_total);
    const MedRanks lo = med_ranks(0, h);
    const MedRanks up = med_ranks(w_total - h, h);
    const long long ks[6] = {med.r1, med.r2, lo.r1, lo.r2, up.r1, up.r2};
    calc_t v[6];
    select_ranks(data, freq, nullptr, 0, 0, ks, 6, v);
    s.median = (v[0] + v[1]) / 2;
    s.q1 = h > 0 ? (v[2] + v[3]) / 2 : NAN;
    s.q3 = h > 0 ? (v[4] + v[5]) / 2 : NAN;
    s.ok = true;
    return s;
}

// ---- Small dense linear solve (normal equations, <= 5x5) --------

constexpr int kMaxDim = 5;

// Solve A x = b in place with partial pivoting. False when singular.
bool solve_linear(calc_t a[kMaxDim][kMaxDim], calc_t* b, int n) {
    for (int col = 0; col < n; ++col) {
        int piv = col;
        for (int r = col + 1; r < n; ++r) {
            if (std::fabs(a[r][col]) > std::fabs(a[piv][col])) {
                piv = r;
            }
        }
        if (std::fabs(a[piv][col]) < 1e-30) {
            return false;
        }
        if (piv != col) {
            for (int c = col; c < n; ++c) {
                const calc_t t = a[col][c];
                a[col][c] = a[piv][c];
                a[piv][c] = t;
            }
            const calc_t t = b[col];
            b[col] = b[piv];
            b[piv] = t;
        }
        for (int r = col + 1; r < n; ++r) {
            const calc_t f = a[r][col] / a[col][col];
            for (int c = col; c < n; ++c) {
                a[r][c] -= f * a[col][c];
            }
            b[r] -= f * b[col];
        }
    }
    for (int r = n - 1; r >= 0; --r) {
        calc_t acc = b[r];
        for (int c = r + 1; c < n; ++c) {
            acc -= a[r][c] * b[c];
        }
        b[r] = acc / a[r][r];
    }
    return true;
}

// ---- Shared fit plumbing ----------------------------------------

// Basic length/finiteness gate shared by every regression. Returns n,
// or -1 with *err set.
int check_pairs(const Array& xs, const Array& ys, const char** err) {
    if (xs.dtype() != Dtype::kDouble || ys.dtype() != Dtype::kDouble) {
        *err = kErrComplex;  // Real-only (D37)
        return -1;
    }
    const int n = xs.size();
    if (ys.size() != n) {
        *err = kErrLen;
        return -1;
    }
    bool finite = true;
    for_each_xy(xs, ys, [&](calc_t x, calc_t y) {
        if (!std::isfinite(x) || !std::isfinite(y)) {
            finite = false;
        }
    });
    if (!finite) {
        *err = kErrNonFinite;
        return -1;
    }
    return n;
}

// 1 - SSE/SST over the original data for a finished fit. NAN when SST
// is degenerate (constant y).
void fill_r_squared(const Array& xs, const Array& ys, RegressionResult* r) {
    const int n = xs.size();
    calc_t sum_y = 0;
    calc_t sum_y2 = 0;
    calc_t sse = 0;
    for_each_xy(xs, ys, [&](calc_t x, calc_t y) {
        sum_y += y;
        sum_y2 += y * y;
        const calc_t d = y - eval_model(r->type, r->coeffs, x);
        sse += d * d;
    });
    const calc_t sst = sum_y2 - sum_y * sum_y / static_cast<calc_t>(n);
    r->r_squared = sst > 0 ? 1 - sse / sst : NAN;
}

// Streaming least squares of Y on X where X/Y are transforms of the
// raw pair. xform/yform: 0 = identity, 1 = ln. Reports the linearized
// fit's intercept A, slope B, and correlation r (the TI convention for
// LnReg/ExpReg/PwrReg diagnostics).
bool lin_fit_transformed(const Array& xs, const Array& ys, int xform, int yform, calc_t* a_out,
                         calc_t* b_out, calc_t* r_out, const char** err) {
    const int n = xs.size();
    if (n < 2) {
        *err = kErrData;
        return false;
    }
    bool domain_ok = true;
    calc_t sx = 0;
    calc_t sy = 0;
    calc_t sxx = 0;
    calc_t syy = 0;
    calc_t sxy = 0;
    for_each_xy(xs, ys, [&](calc_t x, calc_t y) {
        if ((xform == 1 && x <= 0) || (yform == 1 && y <= 0)) {
            domain_ok = false;
            return;
        }
        const calc_t u = xform == 1 ? std::log(x) : x;
        const calc_t v = yform == 1 ? std::log(y) : y;
        sx += u;
        sy += v;
        sxx += u * u;
        syy += v * v;
        sxy += u * v;
    });
    if (!domain_ok) {
        *err = kErrDomain;
        return false;
    }
    const auto nn = static_cast<calc_t>(n);
    const calc_t cxx = sxx - sx * sx / nn;
    const calc_t cyy = syy - sy * sy / nn;
    const calc_t cxy = sxy - sx * sy / nn;
    if (!(cxx > 0)) {
        *err = kErrSingular;
        return false;
    }
    *b_out = cxy / cxx;
    *a_out = (sy - *b_out * sx) / nn;
    *r_out = cyy > 0 ? cxy / std::sqrt(cxx * cyy) : NAN;
    return true;
}

// ---- Polynomial fits (3B.3) -------------------------------------

constexpr int kBinomTable[5][5] = {
    {1, 0, 0, 0, 0}, {1, 1, 0, 0, 0}, {1, 2, 1, 0, 0}, {1, 3, 3, 1, 0}, {1, 4, 6, 4, 1}};

int binom(int n, int k) {
    return kBinomTable[n][k];
}

RegressionResult poly_fit(const Array& xs, const Array& ys, int degree, RegressionType type) {
    RegressionResult res;
    res.type = type;
    const char* err = nullptr;
    const int n = check_pairs(xs, ys, &err);
    if (n < 0) {
        res.error = err;
        return res;
    }
    res.n = n;
    if (n < degree + 1) {
        res.error = kErrData;
        return res;
    }

    // Standardize x (u = (x - cx)/sx) before forming power sums — the
    // normal-equations matrix in raw x^k is hopelessly ill-conditioned
    // for e.g. year-valued data at degree 4.
    calc_t sum_x = 0;
    calc_t sum_x2 = 0;
    for_each_x(xs, [&](calc_t x) {
        sum_x += x;
        sum_x2 += x * x;
    });
    const auto nn = static_cast<calc_t>(n);
    const calc_t cx = sum_x / nn;
    const calc_t var_x = sum_x2 / nn - cx * cx;
    if (!(var_x > 1e-30)) {
        res.error = kErrSingular;
        return res;
    }
    const calc_t sx = std::sqrt(var_x);

    calc_t su[9] = {};  // Σ u^k, k = 0..2d
    calc_t tu[5] = {};  // Σ u^k y, k = 0..d
    calc_t sum_y = 0;
    for_each_xy(xs, ys, [&](calc_t x, calc_t y) {
        const calc_t u = (x - cx) / sx;
        calc_t p = 1;
        for (int k = 0; k <= 2 * degree; ++k) {
            su[k] += p;
            if (k <= degree) {
                tu[k] += p * y;
            }
            p *= u;
        }
        sum_y += y;
    });

    calc_t mat[kMaxDim][kMaxDim];
    calc_t rhs[kMaxDim];
    const int dim = degree + 1;
    for (int i = 0; i < dim; ++i) {
        for (int j = 0; j < dim; ++j) {
            mat[i][j] = su[i + j];
        }
        rhs[i] = tu[i];
    }
    if (!solve_linear(mat, rhs, dim)) {
        res.error = kErrSingular;
        return res;
    }

    // Expand p(u) = Σ b_j u^j back to x coefficients:
    // b_j ((x-cx)/sx)^j = (b_j/sx^j) Σ_k C(j,k) x^k (-cx)^(j-k).
    calc_t a[kMaxDim] = {};
    for (int j = 0; j <= degree; ++j) {
        const calc_t bj = rhs[j] / std::pow(sx, j);
        for (int k = 0; k <= j; ++k) {
            a[k] += bj * static_cast<calc_t>(binom(j, k)) * std::pow(-cx, j - k);
        }
    }
    res.coeff_count = dim;
    for (int k = 0; k <= degree; ++k) {
        res.coeffs[k] = a[degree - k];  // TI order: leading first
    }

    // r only for the linear fit (scale-invariant, so the centered sums
    // serve directly); higher degrees report r² alone.
    if (degree == 1) {
        const calc_t cuy = tu[1] - su[1] * sum_y / nn;
        const calc_t cuu = su[2] - su[1] * su[1] / nn;
        calc_t syy = 0;
        for_each_xy(xs, ys, [&](calc_t, calc_t y) { syy += (y - sum_y / nn) * (y - sum_y / nn); });
        res.r = cuu > 0 && syy > 0 ? cuy / std::sqrt(cuu * syy) : NAN;
    } else {
        res.r = NAN;
    }
    fill_r_squared(xs, ys, &res);
    res.ok = true;
    return res;
}

// ---- Levenberg-Marquardt (3B.5, decision: LM per P3-3) ----------

using ModelFn = calc_t (*)(const calc_t* p, calc_t x);
using JacFn = void (*)(const calc_t* p, calc_t x, calc_t* g);

calc_t sse_of(const Array& xs, const Array& ys, ModelFn f, const calc_t* p) {
    calc_t sse = 0;
    bool bad = false;
    for_each_xy(xs, ys, [&](calc_t x, calc_t y) {
        const calc_t d = y - f(p, x);
        if (!std::isfinite(d)) {
            bad = true;
        }
        sse += d * d;
    });
    return bad ? HUGE_VAL : sse;
}

// Classic Marquardt damping: (JᵀJ + λ·diag(JᵀJ)) δ = Jᵀr, λ down on
// accepted steps, up on rejected ones. Data streams through the chunk
// buffers each pass; np <= 4 so the dense solve is trivial.
bool lm_fit(const Array& xs, const Array& ys, ModelFn f, JacFn jac, calc_t* p, int np,
            bool* converged) {
    constexpr int kMaxIter = 100;
    *converged = false;
    calc_t sse = sse_of(xs, ys, f, p);
    if (!std::isfinite(sse)) {
        return false;
    }
    calc_t lambda = 1e-3;
    for (int iter = 0; iter < kMaxIter; ++iter) {
        calc_t jtj[kMaxDim][kMaxDim] = {};
        calc_t jtr[kMaxDim] = {};
        for_each_xy(xs, ys, [&](calc_t x, calc_t y) {
            calc_t g[kMaxDim];
            jac(p, x, g);
            const calc_t r = y - f(p, x);
            for (int i = 0; i < np; ++i) {
                for (int j = i; j < np; ++j) {
                    jtj[i][j] += g[i] * g[j];
                }
                jtr[i] += g[i] * r;
            }
        });
        for (int i = 0; i < np; ++i) {
            for (int j = 0; j < i; ++j) {
                jtj[i][j] = jtj[j][i];
            }
        }

        bool stepped = false;
        while (lambda < 1e14) {
            calc_t a[kMaxDim][kMaxDim];
            calc_t d[kMaxDim];
            for (int i = 0; i < np; ++i) {
                for (int j = 0; j < np; ++j) {
                    a[i][j] = jtj[i][j];
                }
                a[i][i] = jtj[i][i] * (1 + lambda) + 1e-30;
                d[i] = jtr[i];
            }
            if (solve_linear(a, d, np)) {
                calc_t trial[kMaxDim];
                for (int i = 0; i < np; ++i) {
                    trial[i] = p[i] + d[i];
                }
                const calc_t trial_sse = sse_of(xs, ys, f, trial);
                if (trial_sse < sse) {
                    for (int i = 0; i < np; ++i) {
                        p[i] = trial[i];
                    }
                    const calc_t drop = sse - trial_sse;
                    sse = trial_sse;
                    lambda = lambda > 1e-12 ? lambda * 0.3 : lambda;
                    stepped = true;
                    if (drop <= 1e-12 * (sse + 1e-30)) {
                        *converged = true;
                        return true;
                    }
                    break;
                }
            }
            lambda *= 10;
        }
        if (!stepped) {
            // No downhill step found at any damping — local minimum.
            *converged = true;
            return true;
        }
    }
    return true;  // Iteration cap: usable fit, converged stays false
}

calc_t clamp_exp(calc_t v) {
    return v > 700 ? 700 : (v < -700 ? -700 : v);
}

calc_t logistic_f(const calc_t* p, calc_t x) {
    const calc_t e = std::exp(clamp_exp(-p[1] * x));
    return p[2] / (1 + p[0] * e);
}

void logistic_jac(const calc_t* p, calc_t x, calc_t* g) {
    const calc_t e = std::exp(clamp_exp(-p[1] * x));
    const calc_t den = 1 + p[0] * e;
    g[0] = -p[2] * e / (den * den);
    g[1] = p[2] * p[0] * x * e / (den * den);
    g[2] = 1 / den;
}

calc_t sinusoid_f(const calc_t* p, calc_t x) {
    return p[0] * std::sin(p[1] * x + p[2]) + p[3];
}

void sinusoid_jac(const calc_t* p, calc_t x, calc_t* g) {
    const calc_t s = std::sin(p[1] * x + p[2]);
    const calc_t c = std::cos(p[1] * x + p[2]);
    g[0] = s;
    g[1] = p[0] * x * c;
    g[2] = p[0] * c;
    g[3] = 1;
}

RegressionResult logistic_fit(const Array& xs, const Array& ys) {
    RegressionResult res;
    res.type = RegressionType::kLogistic;
    const char* err = nullptr;
    const int n = check_pairs(xs, ys, &err);
    if (n < 0) {
        res.error = err;
        return res;
    }
    res.n = n;
    if (n < 3) {
        res.error = kErrData;
        return res;
    }
    calc_t max_y = -HUGE_VAL;
    bool pos = true;
    for_each_xy(xs, ys, [&](calc_t, calc_t y) {
        max_y = y > max_y ? y : max_y;
        pos = pos && y > 0;
    });
    if (!pos || !(max_y > 0)) {
        res.error = kErrDomain;
        return res;
    }

    // Seed by the linearized logit at a fixed ceiling guess, then LM
    // refines all three parameters.
    const calc_t c0 = max_y * 1.05;
    calc_t sx = 0;
    calc_t sz = 0;
    calc_t sxx = 0;
    calc_t sxz = 0;
    for_each_xy(xs, ys, [&](calc_t x, calc_t y) {
        const calc_t z = std::log(c0 / y - 1 > 1e-12 ? c0 / y - 1 : 1e-12);
        sx += x;
        sz += z;
        sxx += x * x;
        sxz += x * z;
    });
    const auto nn = static_cast<calc_t>(n);
    const calc_t cxx = sxx - sx * sx / nn;
    if (!(cxx > 0)) {
        res.error = kErrSingular;
        return res;
    }
    const calc_t slope = (sxz - sx * sz / nn) / cxx;
    const calc_t inter = (sz - slope * sx) / nn;
    calc_t p[3] = {std::exp(clamp_exp(inter)), -slope, c0};
    if (!std::isfinite(p[0]) || p[0] <= 0) {
        p[0] = 1;
    }

    bool converged = false;
    if (!lm_fit(xs, ys, logistic_f, logistic_jac, p, 3, &converged)) {
        res.error = kErrSingular;
        return res;
    }
    res.coeffs[0] = p[0];
    res.coeffs[1] = p[1];
    res.coeffs[2] = p[2];
    res.coeff_count = 3;
    res.converged = converged;
    res.r = NAN;
    fill_r_squared(xs, ys, &res);
    res.ok = true;
    return res;
}

RegressionResult sinusoid_fit(const Array& xs, const Array& ys) {
    RegressionResult res;
    res.type = RegressionType::kSinusoidal;
    const char* err = nullptr;
    const int n = check_pairs(xs, ys, &err);
    if (n < 0) {
        res.error = err;
        return res;
    }
    res.n = n;
    if (n < 4) {
        res.error = kErrData;
        return res;
    }
    calc_t min_x = HUGE_VAL;
    calc_t max_x = -HUGE_VAL;
    calc_t min_y = HUGE_VAL;
    calc_t max_y = -HUGE_VAL;
    for_each_xy(xs, ys, [&](calc_t x, calc_t y) {
        min_x = x < min_x ? x : min_x;
        max_x = x > max_x ? x : max_x;
        min_y = y < min_y ? y : min_y;
        max_y = y > max_y ? y : max_y;
    });
    const calc_t span = max_x - min_x;
    if (!(span > 0) || !(max_y > min_y)) {
        res.error = kErrSingular;
        return res;
    }

    // Frequency scan: for each candidate b, y = p·sin(bx) + q·cos(bx)
    // + d is linear — one streaming pass accumulates every candidate's
    // 3x3 normal equations at once, then the best SSE seeds LM.
    constexpr int kMaxCand = 64;
    static calc_t acc[kMaxCand][7];  // ss, sc, cc, s1, c1, ys, yc
    const calc_t max_cycles = static_cast<calc_t>(n) / 2 < 16 ? static_cast<calc_t>(n) / 2 : 16;
    int ncand = 0;
    calc_t bs[kMaxCand];
    for (int q = 2; static_cast<calc_t>(q) * 0.25 <= max_cycles && ncand < kMaxCand; ++q) {
        bs[ncand++] = 2 * kPi * (static_cast<calc_t>(q) * 0.25) / span;
    }
    if (ncand == 0) {
        bs[ncand++] = 2 * kPi / span;
    }
    std::memset(acc, 0, sizeof(acc));
    calc_t sum_y = 0;
    calc_t sum_y2 = 0;
    for_each_xy(xs, ys, [&](calc_t x, calc_t y) {
        sum_y += y;
        sum_y2 += y * y;
        for (int c = 0; c < ncand; ++c) {
            const calc_t s = std::sin(bs[c] * x);
            const calc_t co = std::cos(bs[c] * x);
            acc[c][0] += s * s;
            acc[c][1] += s * co;
            acc[c][2] += co * co;
            acc[c][3] += s;
            acc[c][4] += co;
            acc[c][5] += y * s;
            acc[c][6] += y * co;
        }
    });
    const auto nn = static_cast<calc_t>(n);
    calc_t best_sse = HUGE_VAL;
    calc_t best[4] = {0, bs[0], 0, sum_y / nn};
    for (int c = 0; c < ncand; ++c) {
        calc_t a[kMaxDim][kMaxDim] = {};
        calc_t b[kMaxDim];
        a[0][0] = acc[c][0];
        a[0][1] = acc[c][1];
        a[0][2] = acc[c][3];
        a[1][0] = acc[c][1];
        a[1][1] = acc[c][2];
        a[1][2] = acc[c][4];
        a[2][0] = acc[c][3];
        a[2][1] = acc[c][4];
        a[2][2] = nn;
        b[0] = acc[c][5];
        b[1] = acc[c][6];
        b[2] = sum_y;
        if (!solve_linear(a, b, 3)) {
            continue;
        }
        const calc_t sse = sum_y2 - (b[0] * acc[c][5] + b[1] * acc[c][6] + b[2] * sum_y);
        if (sse < best_sse) {
            best_sse = sse;
            best[0] = std::sqrt(b[0] * b[0] + b[1] * b[1]);
            best[1] = bs[c];
            best[2] = std::atan2(b[1], b[0]);
            best[3] = b[2];
        }
    }
    if (!std::isfinite(best_sse)) {
        res.error = kErrSingular;
        return res;
    }

    calc_t p[4] = {best[0], best[1], best[2], best[3]};
    bool converged = false;
    if (!lm_fit(xs, ys, sinusoid_f, sinusoid_jac, p, 4, &converged)) {
        res.error = kErrSingular;
        return res;
    }
    // Normal form: positive amplitude, phase wrapped into (-pi, pi].
    if (p[0] < 0) {
        p[0] = -p[0];
        p[2] += kPi;
    }
    p[2] = std::remainder(p[2], 2 * kPi);
    res.coeffs[0] = p[0];
    res.coeffs[1] = p[1];
    res.coeffs[2] = p[2];
    res.coeffs[3] = p[3];
    res.coeff_count = 4;
    res.converged = converged;
    res.r = NAN;
    fill_r_squared(xs, ys, &res);
    res.ok = true;
    return res;
}

// ---- Median-median line (3B.6, Tukey three-group) ---------------

RegressionResult medmed_fit(const Array& xs, const Array& ys) {
    RegressionResult res;
    res.type = RegressionType::kMedianMedian;
    const char* err = nullptr;
    const int n = check_pairs(xs, ys, &err);
    if (n < 0) {
        res.error = err;
        return res;
    }
    res.n = n;
    if (n < 3) {
        res.error = kErrData;
        return res;
    }

    // Group sizes by x order (outer groups equal; remainder rule per
    // the TI convention).
    const int k = n / 3;
    const int rem = n % 3;
    const int n1 = rem == 2 ? k + 1 : k;
    const int n3 = n1;
    const int n2 = n - n1 - n3;

    // Everything below is order statistics of x: the two group
    // boundaries plus each group's x-median ranks.
    const MedRanks m1 = med_ranks(0, n1);
    const MedRanks m2 = med_ranks(n1, n2);
    const MedRanks m3 = med_ranks(n1 + n2, n3);
    const long long ks[8] = {n1, n1 + n2 + 1, m1.r1, m1.r2, m2.r1, m2.r2, m3.r1, m3.r2};
    calc_t v[8];
    select_ranks(xs, nullptr, nullptr, 0, 0, ks, 8, v);
    const calc_t bx1 = v[0];  // Largest x in group 1
    const calc_t bx3 = v[1];  // Smallest x in group 3
    const calc_t mx1 = (v[2] + v[3]) / 2;
    const calc_t mx2 = (v[4] + v[5]) / 2;
    const calc_t mx3 = (v[6] + v[7]) / 2;

    // Groups become x-value ranges so the y-medians can be computed by
    // filtered selection. When x ties straddle a positional boundary,
    // the whole tie run lands in the outer group (deviation from a
    // strict positional split — documented behavior).
    const calc_t g2_lo = std::nextafter(bx1, HUGE_VAL);
    const calc_t g2_hi = std::nextafter(bx3, -HUGE_VAL);
    long long w1 = 0;
    long long w2 = 0;
    long long w3 = 0;
    for_each_x(xs, [&](calc_t x) {
        if (x <= bx1) {
            ++w1;
        } else if (x >= bx3) {
            ++w3;
        } else {
            ++w2;
        }
    });

    calc_t my[3] = {NAN, NAN, NAN};
    {
        const MedRanks r1 = med_ranks(0, w1);
        const long long kk[2] = {r1.r1, r1.r2};
        calc_t out[2];
        select_ranks(ys, nullptr, &xs, -HUGE_VAL, bx1, kk, 2, out);
        my[0] = (out[0] + out[1]) / 2;
    }
    if (w2 > 0) {
        const MedRanks r2 = med_ranks(0, w2);
        const long long kk[2] = {r2.r1, r2.r2};
        calc_t out[2];
        select_ranks(ys, nullptr, &xs, g2_lo, g2_hi, kk, 2, out);
        my[1] = (out[0] + out[1]) / 2;
    }
    {
        const MedRanks r3 = med_ranks(0, w3);
        const long long kk[2] = {r3.r1, r3.r2};
        calc_t out[2];
        select_ranks(ys, nullptr, &xs, bx3, HUGE_VAL, kk, 2, out);
        my[2] = (out[0] + out[1]) / 2;
    }

    const calc_t dx = mx3 - mx1;
    if (!(std::fabs(dx) > 1e-30)) {
        res.error = kErrSingular;
        return res;
    }
    const calc_t slope = (my[2] - my[0]) / dx;
    calc_t intercept = 0;
    if (w2 > 0) {
        intercept = ((my[0] - slope * mx1) + (my[1] - slope * mx2) + (my[2] - slope * mx3)) / 3;
    } else {
        intercept = ((my[0] - slope * mx1) + (my[2] - slope * mx3)) / 2;
    }
    res.coeffs[0] = slope;
    res.coeffs[1] = intercept;
    res.coeff_count = 2;
    res.r = NAN;
    fill_r_squared(xs, ys, &res);
    res.ok = true;
    return res;
}

// ---- Linearized fits (3B.4) -------------------------------------

RegressionResult linearized_fit(const Array& xs, const Array& ys, RegressionType type) {
    RegressionResult res;
    res.type = type;
    const char* err = nullptr;
    const int n = check_pairs(xs, ys, &err);
    if (n < 0) {
        res.error = err;
        return res;
    }
    res.n = n;
    const int xform = type == RegressionType::kExponential ? 0 : 1;
    const int yform = type == RegressionType::kLogarithmic ? 0 : 1;
    calc_t a = 0;
    calc_t b = 0;
    calc_t r = 0;
    if (!lin_fit_transformed(xs, ys, xform, yform, &a, &b, &r, &err)) {
        res.error = err;
        return res;
    }
    switch (type) {
        case RegressionType::kLogarithmic:  // y = a + b ln x
            res.coeffs[0] = a;
            res.coeffs[1] = b;
            break;
        case RegressionType::kExponential:  // y = a b^x
            res.coeffs[0] = std::exp(a);
            res.coeffs[1] = std::exp(b);
            break;
        default:  // kPower: y = a x^b
            res.coeffs[0] = std::exp(a);
            res.coeffs[1] = b;
            break;
    }
    res.coeff_count = 2;
    // TI convention: r/r² of the linearized regression, not the raw
    // residuals — matches what the handheld reports for these models.
    res.r = r;
    res.r_squared = std::isnan(r) ? NAN : r * r;
    res.ok = true;
    return res;
}

}  // namespace

OneVarStats one_var(const Array& data) {
    return one_var_impl(data, nullptr);
}

OneVarStats one_var_weighted(const Array& data, const Array& freq) {
    return one_var_impl(data, &freq);
}

TwoVarStats two_var(const Array& x, const Array& y) {
    TwoVarStats s;
    if (x.dtype() != Dtype::kDouble || y.dtype() != Dtype::kDouble) {
        s.error = kErrComplex;  // Real-only (D37)
        return s;
    }
    const int n = x.size();
    if (n == 0) {
        s.error = kErrEmpty;
        return s;
    }
    if (y.size() != n) {
        s.error = kErrLen;
        return s;
    }
    bool finite = true;
    s.min_x = HUGE_VAL;
    s.max_x = -HUGE_VAL;
    s.min_y = HUGE_VAL;
    s.max_y = -HUGE_VAL;
    for_each_xy(x, y, [&](calc_t xv, calc_t yv) {
        if (!std::isfinite(xv) || !std::isfinite(yv)) {
            finite = false;
            return;
        }
        s.sum_x += xv;
        s.sum_y += yv;
        s.sum_xy += xv * yv;
        s.sum_x2 += xv * xv;
        s.sum_y2 += yv * yv;
        s.min_x = xv < s.min_x ? xv : s.min_x;
        s.max_x = xv > s.max_x ? xv : s.max_x;
        s.min_y = yv < s.min_y ? yv : s.min_y;
        s.max_y = yv > s.max_y ? yv : s.max_y;
    });
    if (!finite) {
        s.error = kErrNonFinite;
        return s;
    }
    s.n = n;
    const auto nn = static_cast<calc_t>(n);
    s.mean_x = s.sum_x / nn;
    s.mean_y = s.sum_y / nn;
    const calc_t ssx = s.sum_x2 - nn * s.mean_x * s.mean_x;
    const calc_t ssy = s.sum_y2 - nn * s.mean_y * s.mean_y;
    s.pop_stddev_x = std::sqrt(ssx > 0 ? ssx / nn : 0);
    s.pop_stddev_y = std::sqrt(ssy > 0 ? ssy / nn : 0);
    s.sample_stddev_x = n >= 2 ? std::sqrt(ssx > 0 ? ssx / (nn - 1) : 0) : NAN;
    s.sample_stddev_y = n >= 2 ? std::sqrt(ssy > 0 ? ssy / (nn - 1) : 0) : NAN;
    s.ok = true;
    return s;
}

RegressionResult regress(const Array& x, const Array& y, RegressionType type) {
    switch (type) {
        case RegressionType::kLinear:
            return poly_fit(x, y, 1, type);
        case RegressionType::kQuadratic:
            return poly_fit(x, y, 2, type);
        case RegressionType::kCubic:
            return poly_fit(x, y, 3, type);
        case RegressionType::kQuartic:
            return poly_fit(x, y, 4, type);
        case RegressionType::kLogarithmic:
        case RegressionType::kExponential:
        case RegressionType::kPower:
            return linearized_fit(x, y, type);
        case RegressionType::kLogistic:
            return logistic_fit(x, y);
        case RegressionType::kSinusoidal:
            return sinusoid_fit(x, y);
        default:
            return medmed_fit(x, y);
    }
}

calc_t eval_model(RegressionType type, const calc_t* c, calc_t x) {
    switch (type) {
        case RegressionType::kLinear:
        case RegressionType::kMedianMedian:
            return c[0] * x + c[1];
        case RegressionType::kQuadratic:
            return (c[0] * x + c[1]) * x + c[2];
        case RegressionType::kCubic:
            return ((c[0] * x + c[1]) * x + c[2]) * x + c[3];
        case RegressionType::kQuartic:
            return (((c[0] * x + c[1]) * x + c[2]) * x + c[3]) * x + c[4];
        case RegressionType::kLogarithmic:
            return c[0] + c[1] * std::log(x);
        case RegressionType::kExponential:
            return c[0] * std::pow(c[1], x);
        case RegressionType::kPower:
            return c[0] * std::pow(x, c[1]);
        case RegressionType::kLogistic:
            return logistic_f(c, x);
        default:
            return sinusoid_f(c, x);
    }
}

namespace {

const char* const kRegNames[kRegressionTypeCount] = {"LinReg", "QuadReg", "CubicReg", "QuartReg",
                                                     "LnReg",  "ExpReg",  "PwrReg",   "Logistic",
                                                     "SinReg", "Med-Med"};

const char* const kRegForms[kRegressionTypeCount] = {
    "y=a*x+b", "y=a*x^2+b*x+c", "y=a*x^3+b*x^2+c*x+d", "y=a*x^4+..+d*x+e", "y=a+b*ln(x)",
    "y=a*b^x", "y=a*x^b",       "y=c/(1+a*e^(-b*x))",  "y=a*sin(b*x+c)+d", "y=a*x+b"};

const char* const kCoeffLetters[kMaxDim] = {"a", "b", "c", "d", "e"};

}  // namespace

const char* regression_name(RegressionType t) {
    return kRegNames[static_cast<int>(t)];
}

const char* regression_form(RegressionType t) {
    return kRegForms[static_cast<int>(t)];
}

const char* coeff_name(RegressionType /*t*/, int i) {
    return i >= 0 && i < kMaxDim ? kCoeffLetters[i] : "?";
}

void format_model(const RegressionResult& r, bool degree_trig, char* buf, size_t cap) {
    if (cap == 0) {
        return;
    }
    buf[0] = 0;
    const calc_t* c = r.coeffs;
    switch (r.type) {
        case RegressionType::kLinear:
        case RegressionType::kMedianMedian:
            std::snprintf(buf, cap, "%.10g*x%+.10g", static_cast<double>(c[0]),
                          static_cast<double>(c[1]));
            break;
        case RegressionType::kQuadratic:
            std::snprintf(buf, cap, "%.10g*x^2%+.10g*x%+.10g", static_cast<double>(c[0]),
                          static_cast<double>(c[1]), static_cast<double>(c[2]));
            break;
        case RegressionType::kCubic:
            std::snprintf(buf, cap, "%.10g*x^3%+.10g*x^2%+.10g*x%+.10g", static_cast<double>(c[0]),
                          static_cast<double>(c[1]), static_cast<double>(c[2]),
                          static_cast<double>(c[3]));
            break;
        case RegressionType::kQuartic:
            std::snprintf(buf, cap, "%.10g*x^4%+.10g*x^3%+.10g*x^2%+.10g*x%+.10g",
                          static_cast<double>(c[0]), static_cast<double>(c[1]),
                          static_cast<double>(c[2]), static_cast<double>(c[3]),
                          static_cast<double>(c[4]));
            break;
        case RegressionType::kLogarithmic:
            std::snprintf(buf, cap, "%.10g%+.10g*ln(x)", static_cast<double>(c[0]),
                          static_cast<double>(c[1]));
            break;
        case RegressionType::kExponential:
            std::snprintf(buf, cap, "%.10g*(%.10g)^x", static_cast<double>(c[0]),
                          static_cast<double>(c[1]));
            break;
        case RegressionType::kPower:
            std::snprintf(buf, cap, "%.10g*x^(%.10g)", static_cast<double>(c[0]),
                          static_cast<double>(c[1]));
            break;
        case RegressionType::kLogistic:
            std::snprintf(buf, cap, "%.10g/(1+(%.10g)*exp(-(%.10g)*x))", static_cast<double>(c[2]),
                          static_cast<double>(c[0]), static_cast<double>(c[1]));
            break;
        default: {  // kSinusoidal — sin() in a Y slot follows the
                    // global angle mode, so convert for DEGREE (§10).
            const calc_t k = degree_trig ? static_cast<calc_t>(180.0 / kPi) : 1;
            std::snprintf(buf, cap, "%.10g*sin(%.10g*x%+.10g)%+.10g", static_cast<double>(c[0]),
                          static_cast<double>(c[1] * k), static_cast<double>(c[2] * k),
                          static_cast<double>(c[3]));
            break;
        }
    }
}

}  // namespace math::stats
