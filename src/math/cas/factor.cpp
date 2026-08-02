#include "math/cas/factor.hpp"

#include <cmath>
#include <cstdlib>

#include "math/cas/expand.hpp"
#include "math/cas/poly.hpp"
#include "math/cas/simplify.hpp"

namespace math::cas {

namespace {

constexpr int kMaxDeg = 6;
constexpr long kDivisorLimit = 100000;  // skip integer-root search above this

long igcd(long a, long b) {
    a = std::labs(a);
    b = std::labs(b);
    while (b != 0) {
        const long t = a % b;
        a = b;
        b = t;
    }
    return a;
}

// Horner evaluation of a low-first coefficient array at x.
double poly_eval(const double* a, int deg, double x) {
    double acc = a[deg];
    for (int i = deg - 1; i >= 0; --i) {
        acc = acc * x + a[i];
    }
    return acc;
}

// Divide low-first poly a[0..*deg] by (x - root) in place (root is a root, so
// the remainder is ~0); reduces *deg by one.
void synth_divide(double* a, int* deg, double root) {
    const int d = *deg;
    double q[kMaxDeg + 1];
    q[d - 1] = a[d];
    for (int i = d - 1; i >= 1; --i) {
        q[i - 1] = a[i] + root * q[i];
    }
    for (int i = 0; i < d; ++i) {
        a[i] = q[i];
    }
    *deg = d - 1;
}

Expr* var_pow(char var, int p) {
    if (p == 0) {
        return Expr::num(1.0);
    }
    if (p == 1) {
        return Expr::var(var);
    }
    return Expr::pow(Expr::var(var), Expr::num(p));
}

// Rebuild an expression sum c[i]*var^i from a low-first coefficient array.
Expr* build_poly(const double* a, int deg, char var) {
    Expr* sum = nullptr;
    for (int i = 0; i <= deg; ++i) {
        if (a[i] == 0.0) {
            continue;
        }
        Expr* term = (i == 0)        ? Expr::num(a[i])
                     : (a[i] == 1.0) ? var_pow(var, i)
                                     : Expr::mul(Expr::num(a[i]), var_pow(var, i));
        sum = (sum == nullptr) ? term : Expr::add(sum, term);
    }
    return (sum == nullptr) ? Expr::num(0.0) : sum;
}

}  // namespace

Expr* factor(const Expr* expr, char var) {
    Expr* f = expand(expr);
    if (f == nullptr) {
        return nullptr;
    }
    double c[kMaxDeg + 1];
    int deg = 0;
    if (!poly_coeffs(f, var, c, kMaxDeg, &deg) || deg < 1) {
        return simplify(f);  // not a (>=deg 1) polynomial in var
    }

    // Lowest power of var present -> x^k common factor.
    int k = 0;
    while (k <= deg && c[k] == 0.0) {
        ++k;
    }
    int m = deg - k;
    double r[kMaxDeg + 1];
    for (int i = 0; i <= m; ++i) {
        r[i] = c[k + i];
    }

    // Integer content (GCD of coefficients) when all are integers.
    double content = 1.0;
    bool all_int = true;
    for (int i = 0; i <= m; ++i) {
        if (std::floor(r[i]) != r[i]) {
            all_int = false;
        }
    }
    if (all_int) {
        long g = 0;
        for (int i = 0; i <= m; ++i) {
            g = igcd(g, static_cast<long>(std::llround(r[i])));
        }
        if (g > 1) {
            content = static_cast<double>(g);
            for (int i = 0; i <= m; ++i) {
                r[i] /= content;
            }
        }
    }

    // Extract integer roots (rational-root theorem, q = 1) via synthetic div.
    Expr* lin[kMaxDeg];
    int nlin = 0;
    bool changed = true;
    while (m >= 1 && changed && nlin < kMaxDeg) {
        changed = false;
        const long c0 = static_cast<long>(std::llround(r[0]));
        if (c0 == 0) {
            break;
        }
        const long lim = std::labs(c0);
        if (lim > kDivisorLimit) {
            break;
        }
        for (long d = 1; d <= lim && !changed; ++d) {
            if (c0 % d != 0) {
                continue;
            }
            for (int sgn = -1; sgn <= 1 && !changed; sgn += 2) {
                const auto root = static_cast<double>(sgn * d);
                if (std::fabs(poly_eval(r, m, root)) < 1e-7) {
                    lin[nlin++] = Expr::add(Expr::var(var), Expr::num(-root));
                    synth_divide(r, &m, root);
                    changed = true;
                }
            }
        }
    }

    const bool progressed = (k > 0) || (content != 1.0) || (nlin > 0);
    if (!progressed) {
        return simplify(f);  // irreducible by these methods
    }

    Expr* result = nullptr;
    auto mul_in = [&](Expr* e) { result = (result == nullptr) ? e : Expr::mul(result, e); };
    if (content != 1.0) {
        mul_in(Expr::num(content));
    }
    if (k > 0) {
        mul_in(var_pow(var, k));
    }
    for (int i = 0; i < nlin; ++i) {
        mul_in(lin[i]);
    }
    if (m == 0) {
        if (r[0] != 1.0) {
            mul_in(Expr::num(r[0]));
        }
    } else {
        mul_in(build_poly(r, m, var));
    }
    return simplify(result);
}

}  // namespace math::cas
