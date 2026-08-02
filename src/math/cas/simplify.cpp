#include "math/cas/simplify.hpp"

#include <cmath>
#include <cstring>

namespace math::cas {

namespace {

constexpr int kMaxOperands = 64;  // cap on n-ary width handled in one node
constexpr int kMaxPasses = 50;    // fixed-point cap (spec §13 Risk 1)

Expr* simplify_rec(const Expr* e);

bool is_integer(double v) {
    return std::isfinite(v) && std::floor(v) == v;
}

// ---- Canonical ordering ----------------------------------------------------

int type_rank(const Expr* e) {
    switch (e->type) {
        case ExprType::kNum:
            return 0;
        case ExprType::kVar:
            return 1;
        case ExprType::kFunc:
            return 2;
        case ExprType::kPow:
            return 3;
        case ExprType::kMul:
            return 4;
        case ExprType::kAdd:
            return 5;
        default:
            return 6;
    }
}

bool expr_less(const Expr* a, const Expr* b) {
    const int ra = type_rank(a);
    const int rb = type_rank(b);
    if (ra != rb) {
        return ra < rb;
    }
    switch (a->type) {
        case ExprType::kNum:
            return a->num_val < b->num_val;
        case ExprType::kVar:
            return a->var_name < b->var_name;
        case ExprType::kFunc: {
            const int c = std::strcmp(a->func_name, b->func_name);
            if (c != 0) {
                return c < 0;
            }
            if (a->child != nullptr && b->child != nullptr) {
                return expr_less(a->child, b->child);
            }
            return (a->child == nullptr) && (b->child != nullptr);
        }
        case ExprType::kPow:
            if (!a->child->equals(b->child)) {
                return expr_less(a->child, b->child);
            }
            return expr_less(a->child->next, b->child->next);
        default: {
            const int na = a->child_count();
            const int nb = b->child_count();
            if (na != nb) {
                return na < nb;
            }
            const Expr* ca = a->child;
            const Expr* cb = b->child;
            while (ca != nullptr && cb != nullptr) {
                if (!ca->equals(cb)) {
                    return expr_less(ca, cb);
                }
                ca = ca->next;
                cb = cb->next;
            }
            return false;
        }
    }
}

void sort_exprs(Expr** arr, int n) {
    for (int i = 1; i < n; ++i) {
        Expr* key = arr[i];
        int j = i - 1;
        while (j >= 0 && expr_less(key, arr[j])) {
            arr[j + 1] = arr[j];
            --j;
        }
        arr[j + 1] = key;
    }
}

// Sort terms (rests[]) with their parallel coefficients by canonical order.
void sort_exprs_with_coeffs(Expr** rests, double* coeffs, int n) {
    for (int i = 1; i < n; ++i) {
        Expr* key = rests[i];
        const double kc = coeffs[i];
        int j = i - 1;
        while (j >= 0 && expr_less(key, rests[j])) {
            rests[j + 1] = rests[j];
            coeffs[j + 1] = coeffs[j];
            --j;
        }
        rests[j + 1] = key;
        coeffs[j + 1] = kc;
    }
}

// ---- Builders --------------------------------------------------------------

Expr* make_product(Expr** factors, int n) {
    if (n == 0) {
        return Expr::num(1.0);
    }
    Expr* out = factors[0];
    for (int i = 1; i < n; ++i) {
        out = Expr::mul(out, factors[i]);
        if (out == nullptr) {
            return nullptr;
        }
    }
    return out;
}

Expr* make_sum(Expr** terms, int n) {
    if (n == 0) {
        return Expr::num(0.0);
    }
    Expr* out = terms[0];
    for (int i = 1; i < n; ++i) {
        out = Expr::add(out, terms[i]);
        if (out == nullptr) {
            return nullptr;
        }
    }
    return out;
}

// ---- Function constant folding (exact values only) -------------------------

Expr* fold_func(const char* name, Expr* arg) {
    if (!arg->is_num()) {
        return nullptr;
    }
    const double v = arg->num_val;
    if (std::strcmp(name, "abs") == 0) {
        return Expr::num(std::fabs(v));
    }
    if (v == 0.0) {
        if (std::strcmp(name, "sin") == 0 || std::strcmp(name, "tan") == 0 ||
            std::strcmp(name, "sinh") == 0 || std::strcmp(name, "tanh") == 0 ||
            std::strcmp(name, "asin") == 0 || std::strcmp(name, "atan") == 0 ||
            std::strcmp(name, "sqrt") == 0) {
            return Expr::num(0.0);
        }
        if (std::strcmp(name, "cos") == 0 || std::strcmp(name, "cosh") == 0 ||
            std::strcmp(name, "exp") == 0) {
            return Expr::num(1.0);
        }
    }
    if (v == 1.0) {
        if (std::strcmp(name, "ln") == 0 || std::strcmp(name, "log") == 0 ||
            std::strcmp(name, "acos") == 0) {
            return Expr::num(0.0);
        }
        if (std::strcmp(name, "sqrt") == 0) {
            return Expr::num(1.0);
        }
    }
    return nullptr;
}

// ---- Power ----------------------------------------------------------------

// i^k for integer k, reduced by i^2 = -1. Returns a fresh node.
Expr* reduce_i_power(long k) {
    const long r = ((k % 4) + 4) % 4;
    switch (r) {
        case 0:
            return Expr::num(1.0);
        case 1:
            return Expr::var('i');
        case 2:
            return Expr::num(-1.0);
        default:  // 3 -> -i
            return Expr::mul(Expr::num(-1.0), Expr::var('i'));
    }
}

Expr* simplify_pow(Expr* base, Expr* exp) {
    if (base == nullptr || exp == nullptr) {
        return nullptr;
    }
    if (base->is_num() && base->num_val == 1.0) {
        return Expr::num(1.0);  // 1^anything = 1
    }
    if (!exp->is_num()) {
        return Expr::pow(base, exp);
    }
    const double ev = exp->num_val;
    if (ev == 0.0) {
        return Expr::num(1.0);  // x^0 = 1 (0^0 defined as 1 here)
    }
    if (ev == 1.0) {
        return base;
    }
    if (base->is_num()) {
        if (is_integer(ev)) {
            return Expr::num(std::pow(base->num_val, ev));
        }
        return Expr::pow(base, exp);  // e.g. 2^0.5 stays a surd (Stage 4)
    }
    if (base->is_var() && base->var_name == 'i' && is_integer(ev)) {
        return reduce_i_power(static_cast<long>(ev));
    }
    if (base->is_pow() && base->child->next->is_num()) {
        // (b^e2)^ev = b^(e2*ev)
        return simplify_pow(base->child, Expr::num(base->child->next->num_val * ev));
    }
    if (base->is_mul() && is_integer(ev)) {
        // Distribute over a product base: (a*b)^n = a^n * b^n (linear, safe).
        Expr* prod = nullptr;
        for (const Expr* c = base->child; c != nullptr; c = c->next) {
            Expr* f = simplify_pow(c->clone(), Expr::num(ev));
            if (f == nullptr) {
                return nullptr;
            }
            prod = (prod == nullptr) ? f : Expr::mul(prod, f);
        }
        return simplify_rec(prod);
    }
    return Expr::pow(base, exp);
}

// ---- Product --------------------------------------------------------------

// Classify one already-simplified factor into the coefficient or a
// base/exponent slot (combining like bases). Returns false on overflow.
bool add_factor(Expr* f, double* coeff, Expr** bases, double* exps, int* nb) {
    if (f->is_num()) {
        *coeff *= f->num_val;
        return true;
    }
    Expr* base = f;
    double ex = 1.0;
    if (f->is_pow() && f->child->next->is_num()) {
        base = f->child;
        ex = f->child->next->num_val;
    }
    for (int i = 0; i < *nb; ++i) {
        if (bases[i]->equals(base)) {
            exps[i] += ex;
            return true;
        }
    }
    if (*nb >= kMaxOperands) {
        return false;
    }
    bases[*nb] = base;
    exps[*nb] = ex;
    ++(*nb);
    return true;
}

Expr* simplify_product(const Expr* e) {
    double coeff = 1.0;
    Expr* bases[kMaxOperands];
    double exps[kMaxOperands];
    int nb = 0;

    for (const Expr* c = e->child; c != nullptr; c = c->next) {
        Expr* sc = simplify_rec(c);
        if (sc == nullptr) {
            return nullptr;
        }
        if (sc->is_mul()) {
            for (Expr* f = sc->child; f != nullptr;) {
                Expr* nx = f->next;
                if (!add_factor(f, &coeff, bases, exps, &nb)) {
                    return e->clone();
                }
                f = nx;
            }
        } else if (!add_factor(sc, &coeff, bases, exps, &nb)) {
            return e->clone();
        }
    }

    if (coeff == 0.0) {
        return Expr::num(0.0);  // annihilation
    }

    // i^2 = -1: fold an integer power of i into the coefficient.
    for (int i = 0; i < nb; ++i) {
        if (bases[i]->is_var() && bases[i]->var_name == 'i' && is_integer(exps[i])) {
            const long r = ((static_cast<long>(exps[i]) % 4) + 4) % 4;
            if (r == 2 || r == 3) {
                coeff = -coeff;
            }
            exps[i] = (r == 1 || r == 3) ? 1.0 : 0.0;
        }
    }

    Expr* factors[kMaxOperands];
    int nf = 0;
    for (int i = 0; i < nb; ++i) {
        if (exps[i] == 0.0) {
            continue;
        }
        Expr* f = (exps[i] == 1.0) ? bases[i] : Expr::pow(bases[i], Expr::num(exps[i]));
        if (f == nullptr) {
            return nullptr;
        }
        factors[nf++] = f;
    }
    sort_exprs(factors, nf);

    if (nf == 0) {
        return Expr::num(coeff);
    }
    if (coeff == 1.0) {
        return make_product(factors, nf);
    }
    Expr* parts[kMaxOperands + 1];
    parts[0] = Expr::num(coeff);
    for (int i = 0; i < nf; ++i) {
        parts[i + 1] = factors[i];
    }
    return make_product(parts, nf + 1);
}

// ---- Sum ------------------------------------------------------------------

// Split a term into a numeric coefficient and its (canonical) non-numeric
// "rest". rest == nullptr means the term was a pure number.
void split_term(Expr* t, double* coeff, Expr** rest) {
    if (t->is_num()) {
        *coeff = t->num_val;
        *rest = nullptr;
        return;
    }
    if (t->is_mul() && t->child->is_num()) {
        *coeff = t->child->num_val;
        Expr* factors[kMaxOperands];
        int nf = 0;
        for (const Expr* c = t->child->next; c != nullptr && nf < kMaxOperands; c = c->next) {
            factors[nf++] = c->clone();
        }
        *rest = make_product(factors, nf);
        return;
    }
    *coeff = 1.0;
    *rest = t->clone();
}

bool add_summand(Expr* t, double* constant, Expr** rests, double* coeffs, int* nt) {
    double c = 0.0;
    Expr* rest = nullptr;
    split_term(t, &c, &rest);
    if (rest == nullptr) {
        *constant += c;
        return true;
    }
    for (int i = 0; i < *nt; ++i) {
        if (rests[i]->equals(rest)) {
            coeffs[i] += c;
            return true;
        }
    }
    if (*nt >= kMaxOperands) {
        return false;
    }
    rests[*nt] = rest;
    coeffs[*nt] = c;
    ++(*nt);
    return true;
}

Expr* simplify_sum(const Expr* e) {
    double constant = 0.0;
    Expr* rests[kMaxOperands];
    double coeffs[kMaxOperands];
    int nt = 0;

    for (const Expr* c = e->child; c != nullptr; c = c->next) {
        Expr* sc = simplify_rec(c);
        if (sc == nullptr) {
            return nullptr;
        }
        if (sc->is_add()) {
            for (Expr* t = sc->child; t != nullptr;) {
                Expr* nx = t->next;
                if (!add_summand(t, &constant, rests, coeffs, &nt)) {
                    return e->clone();
                }
                t = nx;
            }
        } else if (!add_summand(sc, &constant, rests, coeffs, &nt)) {
            return e->clone();
        }
    }

    sort_exprs_with_coeffs(rests, coeffs, nt);

    Expr* parts[kMaxOperands + 1];
    int np = 0;
    for (int i = 0; i < nt; ++i) {
        if (coeffs[i] == 0.0) {
            continue;
        }
        Expr* term = (coeffs[i] == 1.0) ? rests[i] : Expr::mul(Expr::num(coeffs[i]), rests[i]);
        if (term == nullptr) {
            return nullptr;
        }
        parts[np++] = term;
    }
    if (constant != 0.0 || np == 0) {
        parts[np++] = Expr::num(constant);
    }
    return make_sum(parts, np);
}

// ---- Driver ---------------------------------------------------------------

Expr* simplify_rec(const Expr* e) {
    if (e == nullptr) {
        return nullptr;
    }
    switch (e->type) {
        case ExprType::kNum:
            return Expr::num(e->num_val);
        case ExprType::kVar:
            return Expr::var(e->var_name);
        case ExprType::kFunc: {
            if (e->child == nullptr) {
                return Expr::func(e->func_name, nullptr);  // named constant (pi)
            }
            Expr* arg = simplify_rec(e->child);
            if (arg == nullptr) {
                return nullptr;
            }
            Expr* folded = fold_func(e->func_name, arg);
            return (folded != nullptr) ? folded : Expr::func(e->func_name, arg);
        }
        case ExprType::kEq: {
            Expr* lhs = simplify_rec(e->child);
            Expr* rhs = simplify_rec(e->child->next);
            if (lhs == nullptr || rhs == nullptr) {
                return nullptr;
            }
            return Expr::eq(lhs, rhs);
        }
        case ExprType::kNeg: {
            Expr* c = e->child->clone();
            if (c == nullptr) {
                return nullptr;
            }
            return simplify_rec(Expr::mul(Expr::num(-1.0), c));
        }
        case ExprType::kPow:
            return simplify_pow(simplify_rec(e->child), simplify_rec(e->child->next));
        case ExprType::kAdd:
            return simplify_sum(e);
        case ExprType::kMul:
            return simplify_product(e);
    }
    return e->clone();
}

}  // namespace

Expr* simplify(const Expr* expr) {
    Expr* cur = simplify_rec(expr);
    for (int pass = 0; pass < kMaxPasses && cur != nullptr; ++pass) {
        Expr* next = simplify_rec(cur);
        if (next == nullptr) {
            return cur;  // pool exhausted; last good form
        }
        if (next->equals(cur)) {
            return next;
        }
        cur = next;
    }
    return cur;
}

}  // namespace math::cas
