#include "math/cas/serialize.hpp"

#include <cmath>
#include <cstdio>

namespace math::cas {

namespace {

// Precedence levels for parenthesization (higher binds tighter).
constexpr int kPrecEq = 0;
constexpr int kPrecAdd = 1;
constexpr int kPrecMul = 2;
constexpr int kPrecUnary = 3;
constexpr int kPrecPow = 4;
constexpr int kPrecAtom = 5;

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
        case ExprType::kNum:
            return e->num_val < 0.0 ? kPrecUnary : kPrecAtom;
        default:
            return kPrecAtom;
    }
}

void put_number(Out& o, double v) {
    char tmp[32];
    if (std::fabs(v) < 1e15 && v == static_cast<double>(static_cast<long long>(v))) {
        std::snprintf(tmp, sizeof(tmp), "%lld", static_cast<long long>(v));
    } else {
        std::snprintf(tmp, sizeof(tmp), "%.17g", v);
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
            bool first = true;
            for (const Expr* c = e->child; c != nullptr; c = c->next) {
                double exp = 0.0;
                const Expr* denom = neg_pow_base(c, &exp);
                if (denom != nullptr && !first) {
                    o.put(" / ");
                    if (exp == -1.0) {
                        write(o, denom, kPrecUnary);
                    } else {
                        write(o, denom, kPrecAtom);
                        o.put_char('^');
                        put_number(o, -exp);
                    }
                } else {
                    if (!first) {
                        o.put_char('*');
                    }
                    write(o, c, kPrecMul);
                }
                first = false;
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
