#include "math/cas/exact.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "math/cas/expr.hpp"
#include "math/cas/parser.hpp"
#include "math/cas/serialize.hpp"
#include "math/cas/simplify.hpp"
#include "math/frac.hpp"

namespace math::cas {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Largest radicand we will factor or display. Bounds both the trial-division
// loop below (d runs to sqrt(kMaxRadicand) = 1000) and the width of a
// serialized result.
constexpr long kMaxRadicand = 1000000;

// Denominator ceiling for recognizing a double as p/q. Must match
// serialize.cpp's kMaxDen: accepting a value the serializer would print as
// "%.17g" would show a decimal in place of the exact form.
constexpr long kMaxDen = 10000;

// G2: literals above this are not "an integer the user typed".
constexpr double kMaxLiteral = 1e9;

// Longer inputs are not worth a speculative parse (this runs on every Enter).
constexpr std::size_t kMaxInputLen = 48;

// Result-shape caps, so an accepted form always fits the home screen's
// result line without panning.
constexpr int kMaxProductFactors = 3;
constexpr int kMaxSumTerms = 4;

// What made a form "interesting" — a bare integer sets none of these and is
// never upgraded (the numeric path already shows it exactly).
constexpr unsigned kFlagRational = 1U;
constexpr unsigned kFlagPi = 2U;
constexpr unsigned kFlagSqrt = 4U;

bool is_int(double v) {
    return std::isfinite(v) && v == std::floor(v);
}

// ---- G2 + G3: literal policy and symbol purity -----------------------------

// Every numeric literal in the *parsed* tree must be an integer (so `2.5`
// stays 2.5 and `0.1+0.2` stays 0.3, rather than being re-rationalized), and
// no variables may appear at all. The latter is not optional: the CAS parser
// has no `ans` or `e`, so `ans` would parse as a*n*s and `e` as a variable
// while the numeric engine gives them values — without this gate that is a
// silent-wrong-answer generator, not just a missed opportunity.
bool literals_ok(const Expr* e) {
    if (e == nullptr) {
        return false;
    }
    switch (e->type) {
        case ExprType::kNum:
            return is_int(e->num_val) && std::fabs(e->num_val) < kMaxLiteral;
        case ExprType::kVar:
        case ExprType::kEq:
            return false;
        default:
            break;
    }
    for (const Expr* c = e->child; c != nullptr; c = c->next) {
        if (!literals_ok(c)) {
            return false;
        }
    }
    return true;
}

// ---- Tree rewriting --------------------------------------------------------

// Rebuild `e` with every child passed through `fn`. Returns a fresh tree, or
// nullptr on pool exhaustion (spec §13 Risk 2).
template <typename Fn>
Expr* map_children(const Expr* e, const Fn& fn) {
    switch (e->type) {
        case ExprType::kNum:
        case ExprType::kVar:
            return e->clone();
        case ExprType::kFunc: {
            if (e->child == nullptr) {
                return e->clone();  // named constant (pi)
            }
            // Expr::func treats a null arg as a named constant, so a failed
            // child must be turned into a failed node explicitly.
            Expr* arg = fn(e->child);
            return arg == nullptr ? nullptr : Expr::func(e->func_name, arg);
        }
        case ExprType::kNeg:
            return Expr::neg(fn(e->child));
        case ExprType::kPow:
            return Expr::pow(fn(e->child), fn(e->child->next));
        case ExprType::kAdd:
        case ExprType::kMul: {
            Expr* acc = nullptr;
            for (const Expr* c = e->child; c != nullptr; c = c->next) {
                Expr* m = fn(c);
                if (m == nullptr) {
                    return nullptr;
                }
                if (acc == nullptr) {
                    acc = m;
                } else {
                    acc = e->is_add() ? Expr::add(acc, m) : Expr::mul(acc, m);
                }
                if (acc == nullptr) {
                    return nullptr;
                }
            }
            return acc;
        }
        default:
            return nullptr;
    }
}

// sqrt(u) -> u^(1/2). Working in power space lets the existing simplifier do
// the factor collection for us: sqrt(2)*sqrt(2) collapses to 2 via the
// like-base merge in simplify_product, and 1/sqrt(2) becomes 2^(-1/2) so
// denominator- and radicand-rationalization share one code path.
Expr* to_pow_form(const Expr* e) {
    if (e == nullptr) {
        return nullptr;
    }
    if (e->is_func() && e->child != nullptr && std::strcmp(e->func_name, "sqrt") == 0) {
        return Expr::pow(to_pow_form(e->child), Expr::num(0.5));
    }
    return map_children(e, to_pow_form);
}

// u^(1/2) -> sqrt(u), the display form. Negative half-powers never reach here
// as numbers (surd_pass rationalizes them); anything else is left alone and
// rejected by the classifier.
Expr* to_sqrt_form(const Expr* e) {
    if (e == nullptr) {
        return nullptr;
    }
    if (e->is_pow() && e->child->next->is_num() && e->child->next->num_val == 0.5) {
        Expr* radicand = to_sqrt_form(e->child);
        return radicand == nullptr ? nullptr : Expr::func("sqrt", radicand);
    }
    return map_children(e, to_sqrt_form);
}

// Pull perfect-square factors out of a numeric radicand: v^(+-1/2) becomes
// (k/q) * m^(1/2) with m square-free. Returns nullptr to mean "leave the node
// alone" (not an error) — the classifier decides what happens next.
Expr* split_radical(double v, double exponent) {
    if (!std::isfinite(v)) {
        return nullptr;
    }
    // sqrt(1/v) == 1/sqrt(v), so a negative half-power is the same problem.
    const double r = exponent > 0.0 ? v : (v == 0.0 ? 0.0 : 1.0 / v);
    if (!(r > 0.0) || !std::isfinite(r)) {
        return nullptr;
    }
    if (r == 1.0) {
        return Expr::num(1.0);
    }

    long p = 0;
    long q = 0;
    if (is_int(r) && r < static_cast<double>(kMaxRadicand)) {
        p = static_cast<long>(r);
        q = 1;
    } else if (!math::frac::decimal_to_fraction(r, kMaxDen, &p, &q) || p <= 0 || q < 1) {
        return nullptr;
    }
    if (p > kMaxRadicand / q) {  // checked before multiplying: long is 32-bit here
        return nullptr;
    }

    // sqrt(p/q) == sqrt(p*q)/q, so one square-free extraction covers both.
    long m = p * q;
    long k = 1;
    for (long d = 2; d * d <= m; ++d) {
        while (m % (d * d) == 0) {
            k *= d;
            m /= d * d;
        }
    }

    const double coeff = static_cast<double>(k) / static_cast<double>(q);
    if (m == 1) {
        return Expr::num(coeff);
    }
    Expr* root = Expr::pow(Expr::num(static_cast<double>(m)), Expr::num(0.5));
    if (root == nullptr || coeff == 1.0) {
        return root;
    }
    return Expr::mul(Expr::num(coeff), root);
}

Expr* surd_pass(const Expr* e) {
    if (e == nullptr) {
        return nullptr;
    }
    if (e->is_pow() && e->child->is_num() && e->child->next->is_num()) {
        const double ev = e->child->next->num_val;
        if (ev == 0.5 || ev == -0.5) {
            Expr* pulled = split_radical(e->child->num_val, ev);
            if (pulled != nullptr) {
                return pulled;
            }
        }
    }
    return map_children(e, surd_pass);
}

// ---- G4 + G5: closed-form classifier ---------------------------------------

// The grammar below is a whitelist walked over the *final display tree*, so
// what is accepted is exactly what gets printed. Each step also accumulates
// the form's value, which makes the G5 agreement check free.

// Would the serializer render this as an exact integer or p/q (rather than
// falling back to a decimal)? Mirrors serialize.cpp's rational_parts.
bool rational_ok(double v) {
    if (!std::isfinite(v)) {
        return false;
    }
    if (is_int(v)) {
        return std::fabs(v) < 1e15;
    }
    long p = 0;
    long q = 0;
    return math::frac::decimal_to_fraction(v, kMaxDen, &p, &q);
}

bool atom_value(const Expr* e, double* out, unsigned* flags) {
    switch (e->type) {
        case ExprType::kNum: {
            if (!rational_ok(e->num_val)) {
                return false;
            }
            if (!is_int(e->num_val)) {
                *flags |= kFlagRational;
            }
            *out = e->num_val;
            return true;
        }
        case ExprType::kFunc: {
            if (e->child == nullptr) {
                if (std::strcmp(e->func_name, "pi") != 0) {
                    return false;
                }
                *flags |= kFlagPi;
                *out = kPi;
                return true;
            }
            if (std::strcmp(e->func_name, "sqrt") != 0 || !e->child->is_num()) {
                return false;
            }
            const double n = e->child->num_val;
            if (!is_int(n) || n < 2.0 || n > static_cast<double>(kMaxRadicand)) {
                return false;
            }
            *flags |= kFlagSqrt;
            *out = std::sqrt(n);
            return true;
        }
        case ExprType::kPow: {
            // 1/pi survives simplify as pi^-1 and serializes as "1 / pi".
            // Restricting the base to a FUNC keeps this one level deep.
            const Expr* base = e->child;
            const Expr* exponent = base->next;
            if (base->type != ExprType::kFunc || !exponent->is_num() || exponent->num_val != -1.0) {
                return false;
            }
            double bv = 0.0;
            if (!atom_value(base, &bv, flags) || bv == 0.0) {
                return false;
            }
            *out = 1.0 / bv;
            return true;
        }
        default:
            return false;
    }
}

// A product of atoms with at most one of each kind — a numeric coefficient, a
// pi and a radical. No nesting: simplify's canonical form has already
// flattened products, so a nested MUL here means something unexpected.
bool product_value(const Expr* e, double* out, unsigned* flags) {
    if (!e->is_mul()) {
        return atom_value(e, out, flags);
    }
    double acc = 1.0;
    int factors = 0;
    int pis = 0;
    int roots = 0;
    int nums = 0;
    for (const Expr* c = e->child; c != nullptr; c = c->next) {
        double v = 0.0;
        unsigned f = 0;
        if (!atom_value(c, &v, &f)) {
            return false;
        }
        if ((f & kFlagPi) != 0) {
            ++pis;
        } else if ((f & kFlagSqrt) != 0) {
            ++roots;
        } else {
            ++nums;
        }
        if (pis > 1 || roots > 1 || nums > 1 || ++factors > kMaxProductFactors) {
            return false;
        }
        *flags |= f;
        acc *= v;
    }
    *out = acc;
    return true;
}

bool sum_value(const Expr* e, double* out, unsigned* flags) {
    if (!e->is_add()) {
        return product_value(e, out, flags);
    }
    double acc = 0.0;
    int terms = 0;
    for (const Expr* c = e->child; c != nullptr; c = c->next) {
        double v = 0.0;
        if (!product_value(c, &v, flags) || ++terms > kMaxSumTerms) {
            return false;
        }
        acc += v;
    }
    if (terms < 2) {
        return false;
    }
    *out = acc;
    return true;
}

}  // namespace

bool exact_form(const char* input, double numeric, char* out, std::size_t out_len) {
    if (input == nullptr || out == nullptr || out_len < 8 || !std::isfinite(numeric)) {
        return false;
    }
    const std::size_t in_len = std::strlen(input);
    if (in_len == 0 || in_len > kMaxInputLen) {
        return false;
    }

    g_cas_pool.reset();
    Expr* tree = parse_expr(input, nullptr);
    if (tree == nullptr || !literals_ok(tree)) {
        return false;
    }

    tree = to_pow_form(tree);
    tree = tree != nullptr ? simplify(tree) : nullptr;
    tree = tree != nullptr ? surd_pass(tree) : nullptr;
    tree = tree != nullptr ? simplify(tree) : nullptr;
    tree = tree != nullptr ? to_sqrt_form(tree) : nullptr;
    if (tree == nullptr) {
        return false;
    }

    // A bare reciprocal (1/pi survives simplify as pi^-1) serializes as
    // "pi^-1"; wrapping it in a product routes it through the serializer's
    // numerator/denominator split instead, so it reads — and typesets — as a
    // fraction. Only the top level needs this; inside a product the split
    // already applies.
    if (tree->is_pow()) {
        Expr* wrapped = Expr::mul(Expr::num(1.0), tree);
        if (wrapped == nullptr) {
            return false;
        }
        tree = wrapped;
    }

    double value = 0.0;
    unsigned flags = 0;
    if (!sum_value(tree, &value, &flags) || flags == 0) {
        return false;
    }
    // G5: the exact form must be the same number the user would otherwise
    // have seen. This is what makes CAS-vs-numeric parser divergence unable
    // to change a displayed answer.
    if (std::fabs(value - numeric) > 1e-9 * std::max(1.0, std::fabs(numeric))) {
        return false;
    }

    char text[64];
    const std::size_t n = expr_to_string(tree, text, sizeof(text));
    if (n == 0 || n + 1 >= sizeof(text) || n + 1 > out_len) {
        return false;  // truncation must never surface as a wrong answer
    }
    std::memcpy(out, text, n + 1);
    return true;
}

}  // namespace math::cas
