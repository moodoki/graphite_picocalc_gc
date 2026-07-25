#include "math/frac.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace math::frac {

namespace {
constexpr double kPi = 3.14159265358979323846;
}  // namespace

bool decimal_to_fraction(double x, long max_den, long* p, long* q) {
    if (!std::isfinite(x) || std::fabs(x) > 1e15 || max_den < 1) {
        return false;
    }
    const bool neg = x < 0;
    const double v = std::fabs(x);
    // Continued-fraction convergents h/k.
    long h0 = 0;
    long h1 = 1;
    long k0 = 1;
    long k1 = 0;
    double frac_part = v;
    for (int it = 0; it < 40; ++it) {
        const double fl = std::floor(frac_part);
        if (fl > 1e15) {
            return false;
        }
        const long a = static_cast<long>(fl);
        const long h2 = a * h1 + h0;
        const long k2 = a * k1 + k0;
        if (k2 > max_den) {
            break;
        }
        h0 = h1;
        h1 = h2;
        k0 = k1;
        k1 = k2;
        const double rem = frac_part - fl;
        if (rem < 1e-12) {
            break;
        }
        frac_part = 1.0 / rem;
    }
    if (k1 < 1) {
        return false;
    }
    const double approx = static_cast<double>(h1) / static_cast<double>(k1);
    const double tol = 1e-9 * (v > 1.0 ? v : 1.0);
    if (std::fabs(v - approx) > tol) {
        return false;
    }
    *p = neg ? -h1 : h1;
    *q = k1;
    return true;
}

bool format_fraction(double x, long max_den, char* buf, size_t buf_len) {
    long p = 0;
    long q = 1;
    if (!decimal_to_fraction(x, max_den, &p, &q)) {
        return false;
    }
    if (q == 1) {
        std::snprintf(buf, buf_len, "%ld", p);
    } else {
        std::snprintf(buf, buf_len, "%ld/%ld", p, q);
    }
    return true;
}

bool pi_multiple(double x, long max_num, long max_den, long* p, long* q) {
    if (!std::isfinite(x) || x == 0.0) {
        return false;
    }
    if (!decimal_to_fraction(x / kPi, max_den, p, q)) {
        return false;
    }
    if (*p == 0 || std::labs(*p) > max_num) {
        return false;
    }
    // Tighter absolute check: the tick value must really be (p/q)*pi.
    const double v = kPi * static_cast<double>(*p) / static_cast<double>(*q);
    return std::fabs(x - v) <= 1e-9 * (std::fabs(v) > 1.0 ? std::fabs(v) : 1.0);
}

}  // namespace math::frac
