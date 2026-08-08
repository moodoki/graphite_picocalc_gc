#include "math/cas/solve.hpp"

#include <cmath>
#include <cstring>

#include "math/cas/expand.hpp"
#include "math/cas/poly.hpp"
#include "math/cas/simplify.hpp"

namespace math::cas {

namespace {

bool near_int(double v) {
    return std::fabs(v - std::round(v)) < 1e-9;
}

// (p/q)*pi as an expression (q != 0). p/q == 0 collapses to 0.
Expr* pi_multiple(double p, double q) {
    if (p == 0.0) {
        return Expr::num(0.0);
    }
    return Expr::mul(Expr::num(p / q), Expr::func("pi", nullptr));
}

// A known trig value and its principal inverse as a pi-multiple (p/q)*pi.
struct TrigEntry {
    double value;
    double p;
    double q;
};

// If v matches a table entry, return (p/q)*pi; otherwise the plain inverse
// call finv(c).
Expr* trig_inverse(const TrigEntry* table, int n, const char* finv, const Expr* c, double v) {
    for (int i = 0; i < n; ++i) {
        if (v == table[i].value) {
            return pi_multiple(table[i].p, table[i].q);
        }
    }
    return Expr::func(finv, c->clone());
}

// Inverse of a one-argument function applied to constant c (already simplified).
// Returns an exact pi-multiple for common trig values, else the plain inverse
// call; nullptr if the function is not invertible here.
Expr* apply_inverse(const char* name, const Expr* c) {
    const double v = c->is_num() ? c->num_val : NAN;
    if (std::strcmp(name, "sin") == 0) {
        static const TrigEntry sin_tbl[] = {
            {0, 0, 1}, {0.5, 1, 6}, {-0.5, -1, 6}, {1, 1, 2}, {-1, -1, 2}};
        return trig_inverse(sin_tbl, 5, "asin", c, v);
    }
    if (std::strcmp(name, "cos") == 0) {
        static const TrigEntry cos_tbl[] = {{1, 0, 1}, {0.5, 1, 3}, {0, 1, 2}, {-1, 1, 1}};
        return trig_inverse(cos_tbl, 4, "acos", c, v);
    }
    if (std::strcmp(name, "tan") == 0) {
        static const TrigEntry tan_tbl[] = {{0, 0, 1}, {1, 1, 4}, {-1, -1, 4}};
        return trig_inverse(tan_tbl, 3, "atan", c, v);
    }
    if (std::strcmp(name, "exp") == 0) {
        return Expr::func("ln", c->clone());
    }
    if (std::strcmp(name, "ln") == 0) {
        return Expr::func("exp", c->clone());
    }
    if (std::strcmp(name, "sqrt") == 0) {
        return Expr::pow(c->clone(), Expr::num(2.0));
    }
    return nullptr;
}

// f(x) = c with f a single invertible function of exactly `var`.
bool try_inverse(const Expr* eq, char var, SolveResult* res) {
    if (!eq->is_eq()) {
        return false;
    }
    const Expr* lhs = eq->child;
    const Expr* rhs = eq->child->next;
    const Expr* g = nullptr;
    const Expr* cst = nullptr;
    if (lhs->contains(var) && !rhs->contains(var)) {
        g = lhs;
        cst = rhs;
    } else if (rhs->contains(var) && !lhs->contains(var)) {
        g = rhs;
        cst = lhs;
    } else {
        return false;
    }
    if (!g->is_func() || g->child == nullptr || !g->child->is_var() || g->child->var_name != var) {
        return false;
    }
    Expr* c = simplify(cst);
    Expr* sol = (c != nullptr) ? apply_inverse(g->func_name, c) : nullptr;
    if (sol == nullptr) {
        return false;
    }
    res->solutions[0] = simplify(sol);
    res->count = 1;
    res->exact = true;
    return true;
}

// Build (re + sign*sqrt(disc)*inv2a) for the real quadratic branch.
Expr* real_root(double re, double sign, double disc, double inv2a) {
    const double s = std::sqrt(disc);
    if (near_int(s)) {
        return Expr::num(re + sign * std::round(s) * inv2a);
    }
    return simplify(Expr::add(
        Expr::num(re), Expr::mul(Expr::num(sign * inv2a), Expr::func("sqrt", Expr::num(disc)))));
}

// Build re + sign*(sqrt(-disc)*inv2a)*i for the complex quadratic branch.
Expr* complex_root(double re, double sign, double disc, double inv2a) {
    const double s = std::sqrt(-disc);
    Expr* im_coeff = near_int(s)
                         ? Expr::num(sign * std::round(s) * inv2a)
                         : Expr::mul(Expr::num(sign * inv2a), Expr::func("sqrt", Expr::num(-disc)));
    return simplify(Expr::add(Expr::num(re), Expr::mul(im_coeff, Expr::var('i'))));
}

void solve_quadratic(double a, double b, double c, bool allow_complex, SolveResult* res) {
    const double disc = b * b - 4.0 * a * c;
    const double inv2a = 1.0 / (2.0 * a);
    const double re = -b * inv2a;
    if (disc == 0.0) {
        res->solutions[0] = Expr::num(re);
        res->count = 1;
        res->exact = true;
        return;
    }
    if (disc > 0.0) {
        res->solutions[0] = real_root(re, 1.0, disc, inv2a);
        res->solutions[1] = real_root(re, -1.0, disc, inv2a);
        res->count = 2;
        res->exact = true;
        return;
    }
    // disc < 0: complex roots.
    res->complex = true;
    if (!allow_complex) {
        res->count = 0;  // REAL mode: no real solution
        return;
    }
    res->solutions[0] = complex_root(re, 1.0, disc, inv2a);
    res->solutions[1] = complex_root(re, -1.0, disc, inv2a);
    res->count = 2;
    res->exact = true;
}

}  // namespace

SolveResult solve(const Expr* equation, char var, bool allow_complex) {
    SolveResult res;
    if (equation == nullptr) {
        return res;
    }

    // Inverse-function isolation: f(x) = c.
    if (try_inverse(equation, var, &res)) {
        return res;
    }

    // Move everything to one side: f = lhs - rhs (or the bare expression).
    Expr* f = nullptr;
    if (equation->is_eq()) {
        f = Expr::add(equation->child->clone(), Expr::neg(equation->child->next->clone()));
    } else {
        f = equation->clone();
    }
    f = expand(f);
    if (f == nullptr) {
        return res;
    }

    double c[7];
    int deg = 0;
    if (!poly_coeffs(f, var, c, 6, &deg)) {
        return res;  // not a numeric polynomial (numeric fallback is UI-layer)
    }
    if (deg == 1) {
        res.solutions[0] = Expr::num(-c[0] / c[1]);
        res.count = 1;
        res.exact = true;
    } else if (deg == 2) {
        solve_quadratic(c[2], c[1], c[0], allow_complex, &res);
    }
    // deg 0 or >= 3: unresolved here (cubic/quartic via factoring, Stage 2d).
    return res;
}

}  // namespace math::cas
