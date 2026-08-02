#include "math/cas/expand.hpp"

#include <cmath>

#include "math/cas/simplify.hpp"

namespace math::cas {

namespace {

constexpr double kExpandExpCap = 20.0;  // spec §7: cap power expansion at n<=20

Expr* expand_rec(const Expr* e);

// Multiply two expanded operands, distributing over any sums:
// (a0+a1+...)*(b0+b1+...) -> sum of ai*bj.
Expr* multiply_expand(Expr* a, Expr* b) {
    if (a == nullptr || b == nullptr) {
        return nullptr;
    }
    Expr* sum = nullptr;
    for (Expr* ai = a->is_add() ? a->child : a; ai != nullptr;) {
        Expr* ai_next = a->is_add() ? ai->next : nullptr;
        for (Expr* bj = b->is_add() ? b->child : b; bj != nullptr;) {
            Expr* bj_next = b->is_add() ? bj->next : nullptr;
            Expr* term = Expr::mul(ai->clone(), bj->clone());
            if (term == nullptr) {
                return nullptr;
            }
            sum = (sum == nullptr) ? term : Expr::add(sum, term);
            if (sum == nullptr) {
                return nullptr;
            }
            bj = bj_next;
        }
        ai = ai_next;
    }
    return (sum == nullptr) ? Expr::num(0.0) : sum;
}

double binomial(int n, int k) {
    double r = 1.0;
    for (int i = 0; i < k; ++i) {
        r = r * (n - i) / (i + 1);
    }
    return r;
}

Expr* expand_pow(const Expr* e) {
    Expr* base = expand_rec(e->child);
    const Expr* exp = e->child->next;
    if (base == nullptr) {
        return nullptr;
    }
    const bool int_pow = exp->is_num() && exp->num_val >= 2.0 && exp->num_val <= kExpandExpCap &&
                         std::floor(exp->num_val) == exp->num_val;
    if (base->is_add() && int_pow) {
        const int n = static_cast<int>(exp->num_val);
        // Binomial theorem for a two-term base: minimal intermediate garbage
        // (the bump pool has no intra-op GC — the iterative path below burns
        // through it for high powers). (a+b)^n = sum C(n,k) a^(n-k) b^k.
        if (base->child_count() == 2) {
            Expr* a = base->child;
            Expr* b = base->child->next;
            Expr* sum = nullptr;
            for (int k = 0; k <= n; ++k) {
                Expr* term = Expr::mul(
                    Expr::mul(Expr::num(binomial(n, k)), Expr::pow(a->clone(), Expr::num(n - k))),
                    Expr::pow(b->clone(), Expr::num(k)));
                if (term == nullptr) {
                    return nullptr;
                }
                sum = (sum == nullptr) ? term : Expr::add(sum, term);
            }
            return simplify(sum);
        }
        // General (multi-term) base: iterative multiply, simplifying each step.
        // Lower ceiling because bump-pool garbage accumulates (spec Risk 2).
        Expr* result = base->clone();
        for (int k = 1; k < n && result != nullptr; ++k) {
            result = multiply_expand(result, base->clone());
            if (result != nullptr) {
                result = simplify(result);
            }
        }
        return result;
    }
    return Expr::pow(base, exp->clone());
}

Expr* expand_rec(const Expr* e) {
    if (e == nullptr) {
        return nullptr;
    }
    switch (e->type) {
        case ExprType::kNum:
            return Expr::num(e->num_val);
        case ExprType::kVar:
            return Expr::var(e->var_name);
        case ExprType::kFunc:
            return Expr::func(e->func_name, e->child != nullptr ? expand_rec(e->child) : nullptr);
        case ExprType::kNeg:
            return Expr::mul(Expr::num(-1.0), expand_rec(e->child));
        case ExprType::kEq:
            return Expr::eq(expand_rec(e->child), expand_rec(e->child->next));
        case ExprType::kAdd: {
            Expr* sum = nullptr;
            for (const Expr* c = e->child; c != nullptr; c = c->next) {
                Expr* ec = expand_rec(c);
                if (ec == nullptr) {
                    return nullptr;
                }
                sum = (sum == nullptr) ? ec : Expr::add(sum, ec);
            }
            return sum;
        }
        case ExprType::kMul: {
            Expr* result = Expr::num(1.0);
            for (const Expr* c = e->child; c != nullptr && result != nullptr; c = c->next) {
                Expr* ec = expand_rec(c);
                if (ec == nullptr) {
                    return nullptr;
                }
                result = multiply_expand(result, ec);
            }
            return result;
        }
        case ExprType::kPow:
            return expand_pow(e);
    }
    return nullptr;
}

}  // namespace

Expr* expand(const Expr* expr) {
    Expr* s = simplify(expr);
    if (s == nullptr) {
        return nullptr;
    }
    Expr* x = expand_rec(s);
    if (x == nullptr) {
        return nullptr;
    }
    return simplify(x);
}

}  // namespace math::cas
