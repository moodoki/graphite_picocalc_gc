#include "math/cas/poly.hpp"

#include <cmath>

namespace math::cas {

namespace {

// Classify a single (expanded) term as coeff * var^power. Returns false if the
// term is not a numeric-coefficient monomial in var (e.g. contains another
// variable or a function of var).
bool monomial(const Expr* t, char var, double* coeff, int* power) {
    switch (t->type) {
        case ExprType::kNum:
            *coeff = t->num_val;
            *power = 0;
            return true;
        case ExprType::kVar:
            if (t->var_name != var) {
                return false;  // a different symbol -> non-numeric coefficient
            }
            *coeff = 1.0;
            *power = 1;
            return true;
        case ExprType::kPow: {
            const Expr* base = t->child;
            const Expr* exp = t->child->next;
            if (!base->is_var() || base->var_name != var || !exp->is_num()) {
                return false;
            }
            const double p = exp->num_val;
            if (p < 0.0 || std::floor(p) != p) {
                return false;
            }
            *coeff = 1.0;
            *power = static_cast<int>(p);
            return true;
        }
        case ExprType::kMul: {
            double c = 1.0;
            int pw = 0;
            for (const Expr* f = t->child; f != nullptr; f = f->next) {
                if (f->is_num()) {
                    c *= f->num_val;
                } else if (f->is_var() && f->var_name == var) {
                    pw += 1;
                } else if (f->is_pow() && f->child->is_var() && f->child->var_name == var &&
                           f->child->next->is_num()) {
                    const double p = f->child->next->num_val;
                    if (p < 0.0 || std::floor(p) != p) {
                        return false;
                    }
                    pw += static_cast<int>(p);
                } else {
                    return false;
                }
            }
            *coeff = c;
            *power = pw;
            return true;
        }
        default:
            return false;
    }
}

}  // namespace

bool poly_coeffs(const Expr* f, char var, double* coeffs, int maxdeg, int* degree) {
    for (int i = 0; i <= maxdeg; ++i) {
        coeffs[i] = 0.0;
    }
    *degree = 0;

    auto accumulate = [&](const Expr* t) -> bool {
        double c = 0.0;
        int p = 0;
        if (!monomial(t, var, &c, &p)) {
            return false;
        }
        if (p > maxdeg) {
            return false;
        }
        coeffs[p] += c;
        return true;
    };

    if (f->is_add()) {
        for (const Expr* t = f->child; t != nullptr; t = t->next) {
            if (!accumulate(t)) {
                return false;
            }
        }
    } else if (!accumulate(f)) {
        return false;
    }

    for (int i = maxdeg; i >= 0; --i) {
        if (coeffs[i] != 0.0) {
            *degree = i;
            break;
        }
    }
    return true;
}

}  // namespace math::cas
