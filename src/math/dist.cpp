#include "math/dist.hpp"

#include <cmath>
#include <limits>

// Cephes primitives (drivers/cephes/, compiled with gamma/erf/erfc
// renamed to cephes_* — none of the symbols below collide with libm).
extern "C" {
double ndtr(double x);
double ndtri(double p);
double igam(double a, double x);
double igamc(double a, double x);
double igami(double a, double y0);
double incbet(double a, double b, double x);
double incbi(double a, double b, double y);
}

namespace math::dist {

namespace {

// M_PI/M_LN2 are POSIX extensions — not guaranteed under -std=c++17.
constexpr calc_t kPi = 3.14159265358979323846;
constexpr calc_t kLn2 = 0.69314718055994530942;

calc_t nan_result() {
    return std::numeric_limits<calc_t>::quiet_NaN();
}

calc_t inf_result() {
    return std::numeric_limits<calc_t>::infinity();
}

bool bad(calc_t v) {
    return std::isnan(v);
}

// TI integer-argument rule: k must be an integer within 1e-9.
bool int_arg(calc_t v, calc_t* out) {
    if (bad(v)) {
        return false;
    }
    const calc_t r = std::nearbyint(v);
    if (std::fabs(v - r) > 1e-9) {
        return false;
    }
    *out = r;
    return true;
}

// Lower-tail (one-sided) CDFs; the public two-sided forms difference
// them (D25).
calc_t normal_cdf_lower(calc_t x, calc_t mean, calc_t sd) {
    const calc_t z = (x - mean) / sd;
    // Saturate far tails before cephes: ndtr squares z internally and
    // overflows to NaN for TI-style +/-1e99 range bounds. ndtr(+/-40)
    // is already 1/0 to well below double epsilon.
    if (z < -40) {
        return 0;
    }
    if (z > 40) {
        return 1;
    }
    return ndtr(z);
}

calc_t t_cdf_lower(calc_t t, calc_t df) {
    if (t == 0) {
        return 0.5;
    }
    if (std::isinf(t)) {
        return t > 0 ? 1.0 : 0.0;
    }
    const calc_t p = 0.5 * incbet(df / 2, 0.5, df / (df + t * t));
    return t > 0 ? 1 - p : p;
}

calc_t chisq_cdf_lower(calc_t x, calc_t df) {
    if (x <= 0) {
        return 0;
    }
    if (std::isinf(x)) {
        return 1;
    }
    return igam(df / 2, x / 2);
}

calc_t f_cdf_lower(calc_t x, calc_t df1, calc_t df2) {
    if (x <= 0) {
        return 0;
    }
    if (std::isinf(x) || std::isinf(df1 * x)) {  // df1*x overflow -> inf/inf below
        return 1;
    }
    return incbet(df1 / 2, df2 / 2, df1 * x / (df1 * x + df2));
}

bool cdf_bounds_ok(calc_t lo, calc_t hi) {
    return !bad(lo) && !bad(hi) && lo <= hi;
}

bool area_ok(calc_t area) {
    return !bad(area) && area > 0 && area < 1;
}

}  // namespace

// ---- Normal ----

calc_t normal_pdf(calc_t x, calc_t mean, calc_t sd) {
    if (bad(x) || !(sd > 0)) {
        return nan_result();
    }
    constexpr calc_t kInvSqrt2Pi = 0.39894228040143268;
    const calc_t z = (x - mean) / sd;
    return kInvSqrt2Pi / sd * std::exp(-0.5 * z * z);
}

calc_t normal_cdf(calc_t lo, calc_t hi, calc_t mean, calc_t sd) {
    if (!cdf_bounds_ok(lo, hi) || !(sd > 0)) {
        return nan_result();
    }
    return normal_cdf_lower(hi, mean, sd) - normal_cdf_lower(lo, mean, sd);
}

calc_t normal_inv(calc_t area, calc_t mean, calc_t sd) {
    if (!area_ok(area) || !(sd > 0)) {
        return nan_result();
    }
    return mean + sd * ndtri(area);
}

// ---- Student's t ----

calc_t t_pdf(calc_t x, calc_t df) {
    if (bad(x) || !(df > 0)) {
        return nan_result();
    }
    const calc_t a = (df + 1) / 2;
    const calc_t ln = std::lgamma(a) - std::lgamma(df / 2) - 0.5 * std::log(df * kPi) -
                      a * std::log1p(x * x / df);
    return std::exp(ln);
}

calc_t t_cdf(calc_t lo, calc_t hi, calc_t df) {
    if (!cdf_bounds_ok(lo, hi) || !(df > 0)) {
        return nan_result();
    }
    return t_cdf_lower(hi, df) - t_cdf_lower(lo, df);
}

calc_t t_inv(calc_t area, calc_t df) {
    if (!area_ok(area) || !(df > 0)) {
        return nan_result();
    }
    if (area == 0.5) {
        return 0;
    }
    const calc_t p = area < 0.5 ? 2 * area : 2 * (1 - area);
    const calc_t x = incbi(df / 2, 0.5, p);
    const calc_t t = std::sqrt(df * (1 - x) / x);
    return area < 0.5 ? -t : t;
}

// ---- Chi-square ----

calc_t chisq_pdf(calc_t x, calc_t df) {
    if (bad(x) || !(df > 0)) {
        return nan_result();
    }
    if (x < 0) {
        return 0;
    }
    if (x == 0) {
        // Density limit at the origin.
        return df < 2 ? inf_result() : (df == 2 ? 0.5 : 0);
    }
    const calc_t h = df / 2;
    return std::exp((h - 1) * std::log(x) - x / 2 - h * kLn2 - std::lgamma(h));
}

calc_t chisq_cdf(calc_t lo, calc_t hi, calc_t df) {
    if (!cdf_bounds_ok(lo, hi) || !(df > 0)) {
        return nan_result();
    }
    return chisq_cdf_lower(hi, df) - chisq_cdf_lower(lo, df);
}

calc_t chisq_inv(calc_t area, calc_t df) {
    if (bad(area) || !(df > 0) || area < 0 || area >= 1) {
        return nan_result();
    }
    if (area == 0) {
        return 0;
    }
    // igami inverts the complemented igamc.
    return 2 * igami(df / 2, 1 - area);
}

// ---- F ----

calc_t f_pdf(calc_t x, calc_t df1, calc_t df2) {
    if (bad(x) || !(df1 > 0) || !(df2 > 0)) {
        return nan_result();
    }
    if (x < 0) {
        return 0;
    }
    const calc_t a = df1 / 2;
    const calc_t b = df2 / 2;
    if (x == 0) {
        // Density limit at the origin.
        return df1 < 2 ? inf_result() : (df1 == 2 ? 1 : 0);
    }
    const calc_t ln_beta = std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);
    const calc_t ln = a * std::log(df1 / df2) + (a - 1) * std::log(x) -
                      (a + b) * std::log1p(df1 * x / df2) - ln_beta;
    return std::exp(ln);
}

calc_t f_cdf(calc_t lo, calc_t hi, calc_t df1, calc_t df2) {
    if (!cdf_bounds_ok(lo, hi) || !(df1 > 0) || !(df2 > 0)) {
        return nan_result();
    }
    return f_cdf_lower(hi, df1, df2) - f_cdf_lower(lo, df1, df2);
}

calc_t f_inv(calc_t area, calc_t df1, calc_t df2) {
    if (bad(area) || !(df1 > 0) || !(df2 > 0) || area < 0 || area >= 1) {
        return nan_result();
    }
    if (area == 0) {
        return 0;
    }
    const calc_t w = incbi(df1 / 2, df2 / 2, area);
    return df2 * w / (df1 * (1 - w));
}

// ---- Binomial ----

calc_t binomial_pmf(calc_t k, calc_t n, calc_t p) {
    calc_t ki = 0;
    calc_t ni = 0;
    if (!int_arg(k, &ki) || !int_arg(n, &ni) || bad(p) || p < 0 || p > 1 || ni < 0) {
        return nan_result();
    }
    if (ki < 0 || ki > ni) {
        return 0;
    }
    if (p == 0) {
        return ki == 0 ? 1 : 0;
    }
    if (p == 1) {
        return ki == ni ? 1 : 0;
    }
    const calc_t ln = std::lgamma(ni + 1) - std::lgamma(ki + 1) - std::lgamma(ni - ki + 1) +
                      ki * std::log(p) + (ni - ki) * std::log1p(-p);
    return std::exp(ln);
}

calc_t binomial_cdf(calc_t k, calc_t n, calc_t p) {
    calc_t ni = 0;
    if (bad(k) || !int_arg(n, &ni) || bad(p) || p < 0 || p > 1 || ni < 0) {
        return nan_result();
    }
    const calc_t ki = std::floor(k);
    if (ki < 0) {
        return 0;
    }
    if (ki >= ni) {
        return 1;
    }
    if (p == 0) {
        return 1;
    }
    if (p == 1) {
        return 0;  // ki < ni here
    }
    // P(X <= k) = I_{1-p}(n-k, k+1)  (cephes bdtr identity)
    return incbet(ni - ki, ki + 1, 1 - p);
}

// ---- Poisson ----

calc_t poisson_pmf(calc_t k, calc_t lambda) {
    calc_t ki = 0;
    if (!int_arg(k, &ki) || bad(lambda) || lambda < 0) {
        return nan_result();
    }
    if (ki < 0) {
        return 0;
    }
    if (lambda == 0) {
        return ki == 0 ? 1 : 0;
    }
    return std::exp(-lambda + ki * std::log(lambda) - std::lgamma(ki + 1));
}

calc_t poisson_cdf(calc_t k, calc_t lambda) {
    if (bad(k) || bad(lambda) || lambda < 0) {
        return nan_result();
    }
    const calc_t ki = std::floor(k);
    if (ki < 0) {
        return 0;
    }
    if (lambda == 0) {
        return 1;
    }
    // P(X <= k) = igamc(k+1, lambda)  (cephes pdtr identity)
    return igamc(ki + 1, lambda);
}

// ---- Geometric (trials until first success) ----

calc_t geometric_pmf(calc_t k, calc_t p) {
    calc_t ki = 0;
    if (!int_arg(k, &ki) || bad(p) || p <= 0 || p > 1) {
        return nan_result();
    }
    if (ki < 1) {
        return 0;
    }
    if (p == 1) {
        return ki == 1 ? 1 : 0;
    }
    return std::exp((ki - 1) * std::log1p(-p)) * p;
}

calc_t geometric_cdf(calc_t k, calc_t p) {
    if (bad(k) || bad(p) || p <= 0 || p > 1) {
        return nan_result();
    }
    const calc_t ki = std::floor(k);
    if (ki < 1) {
        return 0;
    }
    if (p == 1) {
        return 1;
    }
    return -std::expm1(ki * std::log1p(-p));
}

// ---- One-sided inference building blocks (3D) ----

calc_t normal_cdf_1(calc_t z) {
    return normal_cdf_lower(z, 0, 1);
}

calc_t normal_sf(calc_t z) {
    return normal_cdf_lower(-z, 0, 1);  // Symmetry keeps tail precision
}

calc_t t_cdf_1(calc_t t, calc_t df) {
    return t_cdf_lower(t, df);
}

calc_t t_sf(calc_t t, calc_t df) {
    return t_cdf_lower(-t, df);
}

calc_t chisq_sf(calc_t x, calc_t df) {
    if (x <= 0) {
        return 1;
    }
    if (std::isinf(x)) {
        return 0;
    }
    return igamc(df / 2, x / 2);
}

calc_t f_sf(calc_t x, calc_t df1, calc_t df2) {
    if (x <= 0) {
        return 1;
    }
    if (std::isinf(x) || std::isinf(df1 * x)) {
        return 0;
    }
    // cephes fdtrc identity: P(F > x) = I_{d2/(d2+d1 x)}(d2/2, d1/2)
    return incbet(df2 / 2, df1 / 2, df2 / (df2 + df1 * x));
}

}  // namespace math::dist
