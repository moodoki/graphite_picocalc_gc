#include "math/cas/serialize.hpp"

#include <cmath>
#include <cstdio>

#include "math/frac.hpp"

namespace math::cas {

namespace {

// Precedence levels for parenthesization (higher binds tighter).
constexpr int kPrecEq = 0;
constexpr int kPrecAdd = 1;
constexpr int kPrecMul = 2;
constexpr int kPrecUnary = 3;
constexpr int kPrecPow = 4;
constexpr int kPrecAtom = 5;

// Largest denominator we'll recognize when rendering a double coefficient as
// an exact fraction (integrate/simplify produce 1/3, 1/4, 3/2, ... as
// doubles). Above this a value is shown as a plain decimal.
constexpr long kMaxDen = 10000;

// A non-integer value with a tight rational form p/q (q > 1); p carries the
// sign. False for integers and for values with no small-denominator fraction.
bool rational_parts(double v, long* p, long* q) {
    if (v == std::floor(v)) {
        return false;
    }
    return math::frac::decimal_to_fraction(v, kMaxDen, p, q) && *q != 1;
}

struct Out {
    char* buf = nullptr;
    std::size_t cap = 0;
    std::size_t len = 0;

    void put(const char* s) {
        while (*s != '\0' && len + 1 < cap) {
            buf[len++] = *s++;
        }
    }
    void put_char(char c) {
        if (len + 1 < cap) {
            buf[len++] = c;
        }
    }
};

int precedence(const Expr* e) {
    switch (e->type) {
        case ExprType::kEq:
            return kPrecEq;
        case ExprType::kAdd:
            return kPrecAdd;
        case ExprType::kMul:
            return kPrecMul;
        case ExprType::kNeg:
            return kPrecUnary;
        case ExprType::kPow:
            return kPrecPow;
        case ExprType::kNum: {
            if (e->num_val < 0.0) {
                return kPrecUnary;
            }
            long p = 0;
            long q = 0;
            // A p/q rendering binds like a division, so a bare fraction gets
            // parenthesized where an atom is expected (e.g. (1/4)^2).
            return rational_parts(e->num_val, &p, &q) ? kPrecMul : kPrecAtom;
        }
        default:
            return kPrecAtom;
    }
}

void put_number(Out& o, double v) {
    char tmp[48];
    if (std::fabs(v) < 1e15 && v == static_cast<double>(static_cast<long long>(v))) {
        std::snprintf(tmp, sizeof(tmp), "%lld", static_cast<long long>(v));
    } else {
        long p = 0;
        long q = 0;
        if (rational_parts(v, &p, &q)) {
            std::snprintf(tmp, sizeof(tmp), "%ld/%ld", p, q);
        } else {
            std::snprintf(tmp, sizeof(tmp), "%.17g", v);
        }
    }
    o.put(tmp);
}

// Is this child a POW with a negative numeric exponent (a division term)?
const Expr* neg_pow_base(const Expr* e, double* out_exp) {
    if (e->type != ExprType::kPow) {
        return nullptr;
    }
    const Expr* base = e->child;
    const Expr* exp = base->next;
    if (exp->type == ExprType::kNum && exp->num_val < 0.0) {
        *out_exp = exp->num_val;
        return base;
    }
    return nullptr;
}

void write(Out& o, const Expr* e, int parent_prec);

void write_body(Out& o, const Expr* e) {
    switch (e->type) {
        case ExprType::kNum:
            put_number(o, e->num_val);
            break;
        case ExprType::kVar:
            o.put_char(e->var_name);
            break;
        case ExprType::kFunc:
            o.put(e->func_name);
            if (e->child != nullptr) {  // nullptr child = named constant (pi)
                o.put_char('(');
                write(o, e->child, kPrecEq);
                o.put_char(')');
            }
            break;
        case ExprType::kNeg:
            o.put_char('-');
            write(o, e->child, kPrecUnary);
            break;
        case ExprType::kEq:
            write(o, e->child, kPrecAdd);
            o.put(" = ");
            write(o, e->child->next, kPrecAdd);
            break;
        case ExprType::kPow:
            write(o, e->child, kPrecAtom);
            o.put_char('^');
            write(o, e->child->next, kPrecUnary);
            break;
        case ExprType::kAdd: {
            bool first = true;
            for (const Expr* c = e->child; c != nullptr; c = c->next) {
                if (c->type == ExprType::kNeg) {
                    o.put(first ? "-" : " - ");
                    write(o, c->child, kPrecMul);
                } else if (c->type == ExprType::kNum && c->num_val < 0.0) {
                    o.put(first ? "-" : " - ");
                    put_number(o, -c->num_val);
                } else {
                    if (!first) {
                        o.put(" + ");
                    }
                    write(o, c, kPrecMul);
                }
                first = false;
            }
            break;
        }
        case ExprType::kMul: {
            // Render as numerator / denominator. A rational numeric coefficient
            // p/q (q>1) contributes p to the numerator and q to the
            // denominator, so mul(1/4, x^4) prints "x^4 / 4" (which the layout
            // builder typesets as a stacked fraction) rather than "0.25*x^4".
            // Negative-power factors (a/b == a*b^-1) are the other denominators.
            long cp = 0;
            long cq = 1;
            const Expr* frac_coeff = nullptr;
            for (const Expr* c = e->child; c != nullptr; c = c->next) {
                long p = 0;
                long q = 0;
                if (c->type == ExprType::kNum && rational_parts(c->num_val, &p, &q)) {
                    frac_coeff = c;
                    cp = p;
                    cq = q;
                    break;
                }
            }
            int denoms = frac_coeff != nullptr ? 1 : 0;
            for (const Expr* c = e->child; c != nullptr; c = c->next) {
                double exp = 0.0;
                if (neg_pow_base(c, &exp) != nullptr) {
                    ++denoms;
                }
            }

            // Numerator.
            const long num_abs = cp < 0 ? -cp : cp;
            bool nfirst = true;
            if (frac_coeff != nullptr && cp < 0) {
                o.put_char('-');
            }
            if (frac_coeff != nullptr && num_abs != 1) {
                put_number(o, static_cast<double>(num_abs));
                nfirst = false;
            }
            for (const Expr* c = e->child; c != nullptr; c = c->next) {
                double exp = 0.0;
                if (c == frac_coeff || neg_pow_base(c, &exp) != nullptr) {
                    continue;
                }
                if (!nfirst) {
                    o.put_char('*');
                }
                write(o, c, kPrecMul);
                nfirst = false;
            }
            if (nfirst) {
                o.put_char('1');  // numerator was empty (e.g. 1/q, 1/x)
            }

            // Denominator.
            if (denoms > 0) {
                o.put(" / ");
                const bool paren = denoms > 1;
                if (paren) {
                    o.put_char('(');
                }
                bool dfirst = true;
                for (const Expr* c = e->child; c != nullptr; c = c->next) {
                    double exp = 0.0;
                    const Expr* denom = neg_pow_base(c, &exp);
                    if (denom == nullptr) {
                        continue;
                    }
                    if (!dfirst) {
                        o.put_char('*');
                    }
                    if (exp == -1.0) {
                        // kPrecUnary so a product/sum base is parenthesized
                        // (a / b*c != a/(b*c)); a bare var/atom stays unwrapped.
                        write(o, denom, kPrecUnary);
                    } else {
                        write(o, denom, kPrecAtom);
                        o.put_char('^');
                        put_number(o, -exp);
                    }
                    dfirst = false;
                }
                if (frac_coeff != nullptr) {
                    if (!dfirst) {
                        o.put_char('*');
                    }
                    put_number(o, static_cast<double>(cq));
                }
                if (paren) {
                    o.put_char(')');
                }
            }
            break;
        }
    }
}

void write(Out& o, const Expr* e, int parent_prec) {
    if (e == nullptr) {
        return;
    }
    const bool paren = precedence(e) < parent_prec;
    if (paren) {
        o.put_char('(');
    }
    write_body(o, e);
    if (paren) {
        o.put_char(')');
    }
}

}  // namespace

std::size_t expr_to_string(const Expr* expr, char* buf, std::size_t buf_len) {
    if (buf == nullptr || buf_len == 0) {
        return 0;
    }
    Out o{buf, buf_len};
    if (expr != nullptr) {
        write(o, expr, kPrecEq);
    }
    buf[o.len] = '\0';
    return o.len;
}

}  // namespace math::cas
