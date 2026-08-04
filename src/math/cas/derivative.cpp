#include "math/cas/derivative.hpp"

#include <cstring>

#include "math/cas/simplify.hpp"

namespace math::cas {

namespace {

constexpr int kMaxFactors = 64;

Expr* deriv(const Expr* e, char var);

// Chain-rule derivative of func(name, u): returns f'(u) * du, or nullptr if
// the function has no known rule. `u` is the original argument (cloned as
// needed); `du` is its already-computed derivative (consumed once).
Expr* deriv_func(const char* name, const Expr* u, Expr* du) {
    if (du == nullptr) {
        return nullptr;
    }
    auto uc = [&]() { return u->clone(); };
    if (std::strcmp(name, "sin") == 0) {
        return Expr::mul(Expr::func("cos", uc()), du);
    }
    if (std::strcmp(name, "cos") == 0) {
        return Expr::mul(Expr::mul(Expr::num(-1.0), Expr::func("sin", uc())), du);
    }
    if (std::strcmp(name, "tan") == 0) {
        Expr* sec2 = Expr::add(Expr::pow(Expr::func("tan", uc()), Expr::num(2.0)), Expr::num(1.0));
        return Expr::mul(sec2, du);
    }
    if (std::strcmp(name, "exp") == 0) {
        return Expr::mul(Expr::func("exp", uc()), du);
    }
    if (std::strcmp(name, "ln") == 0) {
        return Expr::mul(du, Expr::pow(uc(), Expr::num(-1.0)));
    }
    if (std::strcmp(name, "log") == 0) {  // base-10
        Expr* denom = Expr::mul(uc(), Expr::func("ln", Expr::num(10.0)));
        return Expr::mul(du, Expr::pow(denom, Expr::num(-1.0)));
    }
    if (std::strcmp(name, "sqrt") == 0) {
        Expr* denom = Expr::mul(Expr::num(2.0), Expr::func("sqrt", uc()));
        return Expr::mul(du, Expr::pow(denom, Expr::num(-1.0)));
    }
    if (std::strcmp(name, "asin") == 0 || std::strcmp(name, "acos") == 0) {
        Expr* rad =
            Expr::add(Expr::num(1.0), Expr::mul(Expr::num(-1.0), Expr::pow(uc(), Expr::num(2.0))));
        Expr* recip = Expr::pow(Expr::func("sqrt", rad), Expr::num(-1.0));
        Expr* d = Expr::mul(du, recip);
        return (std::strcmp(name, "acos") == 0) ? Expr::mul(Expr::num(-1.0), d) : d;
    }
    if (std::strcmp(name, "atan") == 0) {
        Expr* denom = Expr::add(Expr::num(1.0), Expr::pow(uc(), Expr::num(2.0)));
        return Expr::mul(du, Expr::pow(denom, Expr::num(-1.0)));
    }
    if (std::strcmp(name, "sinh") == 0) {
        return Expr::mul(Expr::func("cosh", uc()), du);
    }
    if (std::strcmp(name, "cosh") == 0) {
        return Expr::mul(Expr::func("sinh", uc()), du);
    }
    if (std::strcmp(name, "tanh") == 0) {
        Expr* d = Expr::add(
            Expr::num(1.0),
            Expr::mul(Expr::num(-1.0), Expr::pow(Expr::func("tanh", uc()), Expr::num(2.0))));
        return Expr::mul(d, du);
    }
    return nullptr;  // no known rule (e.g. abs)
}

// d/dvar of a product (n-ary): sum_i (f0 * ... * fi' * ... * fn).
//
// The factor list lives in the pool's LIFO scratch, not on the stack (D45).
// GCC inlines this into deriv(), which recurses once per level of tree depth,
// so a stack-local kMaxFactors array is paid at every level — 256 B x the
// parser's nesting cap, on a core-0 stack with a 2 KB declared floor. Same
// hazard the simplifier had.
Expr* deriv_product(const Expr* e, char var) {
    ScratchScope scope;
    auto** factors = static_cast<const Expr**>(
        scope.alloc(sizeof(const Expr*) * kMaxFactors, alignof(const Expr*)));
    if (factors == nullptr) {
        return nullptr;
    }
    int n = 0;
    for (const Expr* c = e->child; c != nullptr; c = c->next) {
        if (n >= kMaxFactors) {
            return nullptr;
        }
        factors[n++] = c;
    }
    Expr* sum = nullptr;
    for (int i = 0; i < n; ++i) {
        Expr* prod = nullptr;
        for (int j = 0; j < n; ++j) {
            Expr* piece = (j == i) ? deriv(factors[j], var) : factors[j]->clone();
            if (piece == nullptr) {
                return nullptr;
            }
            prod = (prod == nullptr) ? piece : Expr::mul(prod, piece);
        }
        if (prod == nullptr) {
            return nullptr;
        }
        sum = (sum == nullptr) ? prod : Expr::add(sum, prod);
    }
    return sum;
}

Expr* deriv_power(const Expr* e, char var) {
    const Expr* u = e->child;
    const Expr* v = e->child->next;
    const bool u_has = u->contains(var);
    const bool v_has = v->contains(var);

    if (!v_has) {
        // Power rule: d(u^v) = v * u^(v-1) * u'  (v constant in var)
        Expr* du = deriv(u, var);
        if (du == nullptr) {
            return nullptr;
        }
        Expr* new_exp = Expr::add(v->clone(), Expr::num(-1.0));
        return Expr::mul(Expr::mul(v->clone(), Expr::pow(u->clone(), new_exp)), du);
    }
    if (!u_has) {
        // Exponential rule: d(a^v) = a^v * ln(a) * v'  (a constant in var)
        Expr* dv = deriv(v, var);
        if (dv == nullptr) {
            return nullptr;
        }
        Expr* base_pow = Expr::pow(u->clone(), v->clone());
        return Expr::mul(Expr::mul(base_pow, Expr::func("ln", u->clone())), dv);
    }
    // General: d(u^v) = u^v * (v'*ln(u) + v*u'/u)
    Expr* du = deriv(u, var);
    Expr* dv = deriv(v, var);
    if (du == nullptr || dv == nullptr) {
        return nullptr;
    }
    Expr* term1 = Expr::mul(dv, Expr::func("ln", u->clone()));
    Expr* term2 = Expr::mul(v->clone(), Expr::mul(du, Expr::pow(u->clone(), Expr::num(-1.0))));
    Expr* factor = Expr::add(term1, term2);
    return Expr::mul(Expr::pow(u->clone(), v->clone()), factor);
}

Expr* deriv(const Expr* e, char var) {
    if (e == nullptr) {
        return nullptr;
    }
    if (!e->contains(var)) {
        return Expr::num(0.0);  // constant in var (numbers, other vars, pi, i)
    }
    switch (e->type) {
        case ExprType::kVar:
            return Expr::num(1.0);  // must be var itself (contains(var) is true)
        case ExprType::kNeg: {
            Expr* d = deriv(e->child, var);
            return (d == nullptr) ? nullptr : Expr::mul(Expr::num(-1.0), d);
        }
        case ExprType::kAdd: {
            Expr* sum = nullptr;
            for (const Expr* c = e->child; c != nullptr; c = c->next) {
                Expr* d = deriv(c, var);
                if (d == nullptr) {
                    return nullptr;
                }
                sum = (sum == nullptr) ? d : Expr::add(sum, d);
            }
            return sum;
        }
        case ExprType::kMul:
            return deriv_product(e, var);
        case ExprType::kPow:
            return deriv_power(e, var);
        case ExprType::kFunc: {
            Expr* du = deriv(e->child, var);
            return deriv_func(e->func_name, e->child, du);
        }
        case ExprType::kEq: {
            Expr* dl = deriv(e->child, var);
            Expr* dr = deriv(e->child->next, var);
            if (dl == nullptr || dr == nullptr) {
                return nullptr;
            }
            return Expr::eq(dl, dr);
        }
        default:
            return nullptr;
    }
}

}  // namespace

Expr* differentiate(const Expr* expr, char var) {
    Expr* raw = deriv(expr, var);
    if (raw == nullptr) {
        return nullptr;
    }
    return simplify(raw);
}

Expr* differentiate_n(const Expr* expr, char var, int n) {
    Expr* cur = simplify(expr);
    for (int i = 0; i < n && cur != nullptr; ++i) {
        cur = differentiate(cur, var);
    }
    return cur;
}

}  // namespace math::cas
