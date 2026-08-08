#include "math/cas/integrate.hpp"

#include <cmath>
#include <cstring>

#include "math/cas/derivative.hpp"
#include "math/cas/expand.hpp"
#include "math/cas/poly.hpp"
#include "math/cas/simplify.hpp"
#include "math/numeric_solve.hpp"

namespace math::cas {

namespace {

constexpr int kMaxFactors = 32;
constexpr double kPi = 3.14159265358979323846;

Expr* integrate_rec(const Expr* e, char var, bool allow_parts);

// Numeric evaluation of an Expr with `var` bound to x (constants and the
// standard function set only). Returns NaN on anything unsupported. Used for
// definite integration.
double eval_const(const Expr* e, char var, double x) {
    switch (e->type) {
        case ExprType::kNum:
            return e->num_val;
        case ExprType::kVar:
            return e->var_name == var ? x : NAN;
        case ExprType::kNeg:
            return -eval_const(e->child, var, x);
        case ExprType::kAdd: {
            double s = 0.0;
            for (const Expr* c = e->child; c != nullptr; c = c->next) {
                s += eval_const(c, var, x);
            }
            return s;
        }
        case ExprType::kMul: {
            double p = 1.0;
            for (const Expr* c = e->child; c != nullptr; c = c->next) {
                p *= eval_const(c, var, x);
            }
            return p;
        }
        case ExprType::kPow:
            return std::pow(eval_const(e->child, var, x), eval_const(e->child->next, var, x));
        case ExprType::kFunc: {
            if (e->child == nullptr) {
                return std::strcmp(e->func_name, "pi") == 0 ? kPi : NAN;
            }
            const double a = eval_const(e->child, var, x);
            struct FnEntry {
                const char* name;
                double (*fn)(double);
            };
            static const FnEntry fns[] = {
                {"sin", [](double v) { return std::sin(v); }},
                {"cos", [](double v) { return std::cos(v); }},
                {"tan", [](double v) { return std::tan(v); }},
                {"exp", [](double v) { return std::exp(v); }},
                {"ln", [](double v) { return std::log(v); }},
                {"log", [](double v) { return std::log10(v); }},
                {"sqrt", [](double v) { return std::sqrt(v); }},
                {"asin", [](double v) { return std::asin(v); }},
                {"acos", [](double v) { return std::acos(v); }},
                {"atan", [](double v) { return std::atan(v); }},
                {"sinh", [](double v) { return std::sinh(v); }},
                {"cosh", [](double v) { return std::cosh(v); }},
                {"tanh", [](double v) { return std::tanh(v); }},
                {"abs", [](double v) { return std::fabs(v); }},
            };
            for (const FnEntry& entry : fns) {
                if (std::strcmp(e->func_name, entry.name) == 0) {
                    return entry.fn(a);
                }
            }
            return NAN;
        }
        default:
            return NAN;
    }
}

// If e is linear in var (a*var + b with numeric a,b), return true and set a,b.
bool is_linear(const Expr* e, char var, double* a, double* b) {
    Expr* x = expand(e);
    if (x == nullptr) {
        return false;
    }
    double c[2];
    int deg = 0;
    if (!poly_coeffs(x, var, c, 1, &deg)) {
        return false;
    }
    *a = c[1];
    *b = c[0];
    return true;
}

// Antiderivative log. Uses ln(u) (not ln|u|) so the result stays
// differentiable by our own engine (abs has no derivative rule) and matches
// what the fundamental-theorem check expects; the domain caveat is standard.
Expr* ln_anti(Expr* u) {
    return Expr::func("ln", u);
}

// Integral of a single function application f(u), u linear in var (u = a*var+b).
Expr* integrate_func(const Expr* f, char var) {
    const char* name = f->func_name;
    double a = 0.0;
    double b = 0.0;
    if (f->child == nullptr || !is_linear(f->child, var, &a, &b) || a == 0.0) {
        return nullptr;  // nonlinear argument (e.g. sin(x^2)) is out of scope
    }
    const double inv = 1.0 / a;
    Expr* u = f->child->clone();
    if (std::strcmp(name, "sin") == 0) {
        return Expr::mul(Expr::num(-inv), Expr::func("cos", u));
    }
    if (std::strcmp(name, "cos") == 0) {
        return Expr::mul(Expr::num(inv), Expr::func("sin", u));
    }
    if (std::strcmp(name, "exp") == 0) {
        return Expr::mul(Expr::num(inv), Expr::func("exp", u));
    }
    if (std::strcmp(name, "ln") == 0) {
        // (1/a)*(u*ln(u) - u)
        Expr* body = Expr::add(Expr::mul(u->clone(), Expr::func("ln", u->clone())),
                               Expr::mul(Expr::num(-1.0), u->clone()));
        return Expr::mul(Expr::num(inv), body);
    }
    if (std::strcmp(name, "sqrt") == 0) {
        // (1/a)*(2/3)*u^(3/2)
        return Expr::mul(Expr::num(inv * 2.0 / 3.0), Expr::pow(u, Expr::num(1.5)));
    }
    if (std::strcmp(name, "tan") == 0) {
        // (1/a)*(-ln|cos u|)
        return Expr::mul(Expr::num(-inv), ln_anti(Expr::func("cos", u)));
    }
    return nullptr;
}

Expr* integrate_pow(const Expr* e, char var) {
    const Expr* base = e->child;
    const Expr* exp = e->child->next;

    if (exp->is_num()) {
        const double n = exp->num_val;
        double a = 0.0;
        double b = 0.0;
        if (base->is_var() && base->var_name == var) {
            if (n == -1.0) {
                return ln_anti(Expr::var(var));
            }
            return Expr::mul(Expr::num(1.0 / (n + 1.0)),
                             Expr::pow(Expr::var(var), Expr::num(n + 1.0)));
        }
        if (is_linear(base, var, &a, &b) && a != 0.0 && base->contains(var)) {
            if (n == -1.0) {
                return Expr::mul(Expr::num(1.0 / a), ln_anti(base->clone()));
            }
            return Expr::mul(Expr::num(1.0 / (a * (n + 1.0))),
                             Expr::pow(base->clone(), Expr::num(n + 1.0)));
        }
        return nullptr;  // nonlinear base (power-rule substitution not implemented)
    }

    // a^(kx+b): base constant in var, exponent linear.
    double k = 0.0;
    double b = 0.0;
    if (!base->contains(var) && is_linear(exp, var, &k, &b) && k != 0.0) {
        Expr* recip_ln = Expr::pow(Expr::func("ln", base->clone()), Expr::num(-1.0));
        return Expr::mul(Expr::num(1.0 / k),
                         Expr::mul(Expr::pow(base->clone(), exp->clone()), recip_ln));
    }
    return nullptr;
}

// LIATE priority: lower integrates as u in by-parts (L<I<A<T<E).
int liate_rank(const Expr* f) {
    if (f->is_func()) {
        const char* n = f->func_name;
        if (std::strcmp(n, "ln") == 0 || std::strcmp(n, "log") == 0) {
            return 0;  // L
        }
        if (std::strcmp(n, "asin") == 0 || std::strcmp(n, "acos") == 0 ||
            std::strcmp(n, "atan") == 0) {
            return 1;  // I
        }
        if (std::strcmp(n, "exp") == 0) {
            return 4;  // E
        }
        return 3;  // T (trig/other)
    }
    if (f->is_pow() && !f->child->is_var()) {
        return 4;  // a^x style
    }
    return 2;  // algebraic (var, x^n, polynomial)
}

Expr* integrate_by_parts(const Expr* const* vf, int nvf, char var) {
    // Pick u = lowest LIATE rank; dv = product of the rest.
    int iu = 0;
    for (int i = 1; i < nvf; ++i) {
        if (liate_rank(vf[i]) < liate_rank(vf[iu])) {
            iu = i;
        }
    }
    const Expr* u = vf[iu];
    Expr* dv = nullptr;
    for (int i = 0; i < nvf; ++i) {
        if (i == iu) {
            continue;
        }
        dv = (dv == nullptr) ? vf[i]->clone() : Expr::mul(dv, vf[i]->clone());
    }
    if (dv == nullptr) {
        return nullptr;
    }
    Expr* v = integrate_rec(dv, var, false);
    if (v == nullptr) {
        return nullptr;
    }
    Expr* du = differentiate(u, var);
    if (du == nullptr) {
        return nullptr;
    }
    Expr* rest = integrate_rec(Expr::mul(v->clone(), du), var, false);
    if (rest == nullptr) {
        return nullptr;  // by-parts did not close
    }
    return Expr::add(Expr::mul(u->clone(), v), Expr::mul(Expr::num(-1.0), rest));
}

Expr* integrate_mul(const Expr* e, char var, bool allow_parts) {
    Expr* konst = nullptr;
    const Expr* vf[kMaxFactors];
    int nvf = 0;
    for (const Expr* c = e->child; c != nullptr; c = c->next) {
        if (c->contains(var)) {
            if (nvf >= kMaxFactors) {
                return nullptr;
            }
            vf[nvf++] = c;
        } else {
            konst = (konst == nullptr) ? c->clone() : Expr::mul(konst, c->clone());
        }
    }
    if (konst == nullptr) {
        konst = Expr::num(1.0);
    }
    if (nvf == 0) {
        return Expr::mul(konst, Expr::var(var));  // integral of a constant
    }
    if (nvf == 1) {
        Expr* inner = integrate_rec(vf[0], var, allow_parts);
        return (inner == nullptr) ? nullptr : Expr::mul(konst, inner);
    }
    if (!allow_parts) {
        return nullptr;
    }
    Expr* parts = integrate_by_parts(vf, nvf, var);
    return (parts == nullptr) ? nullptr : Expr::mul(konst, parts);
}

Expr* integrate_rec(const Expr* e, char var, bool allow_parts) {
    if (e == nullptr) {
        return nullptr;
    }
    if (!e->contains(var)) {
        return Expr::mul(e->clone(), Expr::var(var));  // ∫ c dx = c*x
    }
    switch (e->type) {
        case ExprType::kVar:
            return Expr::mul(Expr::num(0.5), Expr::pow(Expr::var(var), Expr::num(2.0)));
        case ExprType::kNeg: {
            Expr* i = integrate_rec(e->child, var, allow_parts);
            return (i == nullptr) ? nullptr : Expr::mul(Expr::num(-1.0), i);
        }
        case ExprType::kAdd: {
            Expr* sum = nullptr;
            for (const Expr* c = e->child; c != nullptr; c = c->next) {
                Expr* i = integrate_rec(c, var, allow_parts);
                if (i == nullptr) {
                    return nullptr;
                }
                sum = (sum == nullptr) ? i : Expr::add(sum, i);
            }
            return sum;
        }
        case ExprType::kMul:
            return integrate_mul(e, var, allow_parts);
        case ExprType::kPow:
            return integrate_pow(e, var);
        case ExprType::kFunc:
            return integrate_func(e, var);
        default:
            return nullptr;
    }
}

}  // namespace

Expr* integrate(const Expr* expr, char var) {
    Expr* raw = integrate_rec(expr, var, true);
    return (raw == nullptr) ? nullptr : simplify(raw);
}

namespace {
struct NumIntegrand {
    const Expr* e;
    char var;
};
double num_eval(void* ctx, calc_t x) {
    auto* n = static_cast<NumIntegrand*>(ctx);
    return eval_const(n->e, n->var, x);
}
}  // namespace

DefIntResult definite_integrate(const Expr* expr, char var, const Expr* lower, const Expr* upper) {
    DefIntResult res;
    const double lo = eval_const(lower, var, 0.0);
    const double hi = eval_const(upper, var, 0.0);

    Expr* anti = integrate(expr, var);
    if (anti != nullptr) {
        res.has_symbolic = true;
        res.antideriv = anti;
        if (!std::isnan(lo) && !std::isnan(hi)) {
            const double fv = eval_const(anti, var, hi) - eval_const(anti, var, lo);
            if (!std::isnan(fv)) {
                res.has_numeric = true;
                res.numeric_val = fv;
                return res;
            }
        }
    }
    // Numeric fallback: Gauss-Kronrod (Phase 4B), evaluating the Expr directly.
    if (!std::isnan(lo) && !std::isnan(hi)) {
        NumIntegrand ctx{expr, var};
        const IntegralResult ir = numeric_integral_fn(num_eval, &ctx, lo, hi);
        res.numeric_val = ir.value;
        res.has_numeric = ir.converged;
    }
    return res;
}

}  // namespace math::cas
