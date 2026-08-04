// Host-side tests for the CAS foundations (Phase 5 Stage 0, tasks 4D.1-4D.3):
// the Expr tree + pool (expr.cpp), the recursive-descent parser (parser.cpp),
// and expr_to_string (serialize.cpp). No numeric evaluation yet — that starts
// with the Stage 1 simplifier.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "math/cas/cas_eval.hpp"
#include "math/cas/derivative.hpp"
#include "math/cas/exact.hpp"
#include "math/cas/expand.hpp"
#include "math/cas/expr.hpp"
#include "math/cas/factor.hpp"
#include "math/cas/integrate.hpp"
#include "math/cas/parser.hpp"
#include "math/cas/serialize.hpp"
#include "math/cas/simplify.hpp"
#include "math/cas/solve.hpp"
#include "math/engine.hpp"
#include "math/scratch.hpp"
#include "math/types.hpp"

using math::cas::differentiate;
using math::cas::expand;
using math::cas::Expr;
using math::cas::factor;
using math::cas::integrate;
using math::cas::ExprType;
using math::cas::g_cas_pool;
using math::cas::parse_expr;
using math::cas::simplify;
using math::cas::solve;
using math::cas::SolveResult;

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

// parse(s) succeeds and its serialization re-parses to a structurally equal
// tree (round-trip stability — spec §4 task 4D.3).
void check_roundtrip(const char* s) {
    g_cas_pool.reset();
    const char* err = nullptr;
    Expr* t1 = parse_expr(s, &err);
    if (t1 == nullptr) {
        ++g_checks;
        ++g_failures;
        std::printf("FAIL: parse(\"%s\") -> null (%s)\n", s, err ? err : "?");
        return;
    }
    char buf[256];
    math::cas::expr_to_string(t1, buf, sizeof(buf));
    Expr* t2 = parse_expr(buf, &err);
    ++g_checks;
    if (t2 == nullptr) {
        ++g_failures;
        std::printf("FAIL: reparse(\"%s\") of \"%s\" -> null (%s)\n", buf, s, err ? err : "?");
        return;
    }
    if (!t1->equals(t2)) {
        ++g_failures;
        std::printf("FAIL: round-trip \"%s\" -> \"%s\" not structurally equal\n", s, buf);
    }
}

// parse(a) and parse(b) yield structurally equal trees.
void check_same(const char* a, const char* b) {
    g_cas_pool.reset();
    Expr* ta = parse_expr(a, nullptr);
    Expr* tb = parse_expr(b, nullptr);
    ++g_checks;
    if (ta == nullptr || tb == nullptr) {
        ++g_failures;
        std::printf("FAIL: parse null in check_same(\"%s\",\"%s\")\n", a, b);
        return;
    }
    if (!ta->equals(tb)) {
        ++g_failures;
        std::printf("FAIL: \"%s\" != \"%s\" structurally\n", a, b);
    }
}

// Numeric evaluator for host tests: evaluates an Expr with a single variable
// bound to xval (used to check derivatives/integrals by value rather than by
// fragile canonical form). Returns NaN on anything it can't evaluate.
double eval(const Expr* e, char var, double xval) {
    switch (e->type) {
        case ExprType::kNum:
            return e->num_val;
        case ExprType::kVar:
            return e->var_name == var ? xval : NAN;
        case ExprType::kNeg:
            return -eval(e->child, var, xval);
        case ExprType::kAdd: {
            double s = 0.0;
            for (const Expr* c = e->child; c != nullptr; c = c->next) {
                s += eval(c, var, xval);
            }
            return s;
        }
        case ExprType::kMul: {
            double p = 1.0;
            for (const Expr* c = e->child; c != nullptr; c = c->next) {
                p *= eval(c, var, xval);
            }
            return p;
        }
        case ExprType::kPow:
            return std::pow(eval(e->child, var, xval), eval(e->child->next, var, xval));
        case ExprType::kFunc: {
            if (e->child == nullptr) {
                return std::strcmp(e->func_name, "pi") == 0 ? M_PI : NAN;
            }
            const double a = eval(e->child, var, xval);
            const char* n = e->func_name;
            if (!std::strcmp(n, "sin")) return std::sin(a);
            if (!std::strcmp(n, "cos")) return std::cos(a);
            if (!std::strcmp(n, "tan")) return std::tan(a);
            if (!std::strcmp(n, "exp")) return std::exp(a);
            if (!std::strcmp(n, "ln")) return std::log(a);
            if (!std::strcmp(n, "log")) return std::log10(a);
            if (!std::strcmp(n, "sqrt")) return std::sqrt(a);
            if (!std::strcmp(n, "asin")) return std::asin(a);
            if (!std::strcmp(n, "acos")) return std::acos(a);
            if (!std::strcmp(n, "atan")) return std::atan(a);
            if (!std::strcmp(n, "sinh")) return std::sinh(a);
            if (!std::strcmp(n, "cosh")) return std::cosh(a);
            if (!std::strcmp(n, "tanh")) return std::tanh(a);
            return NAN;
        }
        default:
            return NAN;
    }
}

// differentiate(input) matches expected by value at several sample points.
void check_deriv(const char* input, char var, const char* expected) {
    g_cas_pool.reset();
    Expr* in = parse_expr(input, nullptr);
    Expr* ex = parse_expr(expected, nullptr);
    ++g_checks;
    if (in == nullptr || ex == nullptr) {
        ++g_failures;
        std::printf("FAIL: parse null in check_deriv(\"%s\")\n", input);
        return;
    }
    Expr* d = differentiate(in, var);
    if (d == nullptr) {
        ++g_failures;
        std::printf("FAIL: differentiate(\"%s\") -> null\n", input);
        return;
    }
    const double xs[] = {0.3, 0.7, 1.3, 2.1};
    for (double x : xs) {
        const double got = eval(d, var, x);
        const double want = eval(ex, var, x);
        if (std::isnan(got) || std::isnan(want) || std::fabs(got - want) > 1e-6) {
            ++g_failures;
            char buf[128];
            math::cas::expr_to_string(d, buf, sizeof(buf));
            std::printf("FAIL: d/d%c \"%s\" = \"%s\": at %g got %.10g want %.10g\n", var, input,
                        buf, x, got, want);
            return;
        }
    }
}

void test_tree_ops() {
    g_cas_pool.reset();
    // Build 2*x^2 + 3*x - 1 by hand.
    Expr* t = Expr::add(Expr::add(Expr::mul(Expr::num(2), Expr::pow(Expr::var('x'), Expr::num(2))),
                                  Expr::mul(Expr::num(3), Expr::var('x'))),
                        Expr::neg(Expr::num(1)));
    check(t != nullptr, "hand-built tree non-null");
    check(t->is_add(), "top is ADD");
    check(t->child_count() == 3, "ADD flattened to 3 children");
    check(t->contains('x'), "contains x");
    check(!t->contains('y'), "does not contain y");

    Expr* c = t->clone();
    check(c != nullptr, "clone non-null");
    check(c != t, "clone is a distinct node");
    check(t->equals(c), "clone equals original");

    // Structural inequality.
    Expr* other = Expr::add(Expr::var('x'), Expr::num(1));
    check(!t->equals(other), "different trees not equal");

    // Predicates on atoms.
    check(Expr::num(0)->is_zero(), "num(0) is_zero");
    check(Expr::num(1)->is_one(), "num(1) is_one");
    check(Expr::num(-1)->is_neg_one(), "num(-1) is_neg_one");
    check(Expr::var('i')->is_var(), "var(i) is_var");
}

void test_parse_structure() {
    g_cas_pool.reset();
    const char* err = nullptr;
    Expr* t = parse_expr("2*x^2 + 3*x - 1", &err);
    check(t != nullptr, "parse quadratic non-null");
    if (t != nullptr) {
        check(t->is_add(), "quadratic top is ADD");
        check(t->child_count() == 3, "quadratic has 3 terms");
        // First term 2*x^2 is a MUL of 2 and x^2.
        Expr* term0 = t->child;
        check(term0->is_mul(), "first term is MUL");
        check(term0->child_count() == 2, "first term MUL has 2 factors");
    }
    // Equation form.
    Expr* eq = parse_expr("x^2 = 4", &err);
    check(eq != nullptr && eq->is_eq(), "equation parses to EQ");
}

void test_roundtrips() {
    const char* cases[] = {
        "2*x^2 + 3*x - 1", "x^2 + 1",     "(x+1)*(x-1)",  "sin(x) + cos(x)",
        "2*x/(4*x^2)",     "-x + 1",      "x^-1",         "3 + 2*i",
        "pi",              "2*pi",        "a = b + c",    "x^2^3",
        "sqrt(x+1)",       "ln(x)/x",     "-(x + 1)",     "2*(x + 3)^4",
    };
    for (const char* s : cases) {
        check_roundtrip(s);
    }
}

void test_implicit_mult() {
    check_same("2x", "2*x");
    check_same("xy", "x*y");
    check_same("2(x+1)", "2*(x+1)");
    check_same("(x+1)(x-1)", "(x+1)*(x-1)");
    check_same("3x^2", "3*x^2");
    check_same("2pi", "2*pi");
}

void test_encodings() {
    // Subtraction encodes as ADD(a, NEG(b)).
    check_same("a - b", "a + (-b)");
    // Right-associative exponentiation.
    check_same("2^3^2", "2^(3^2)");
    check(!parse_expr("2^3^2", nullptr)->equals(parse_expr("(2^3)^2", nullptr)),
          "^ is right-associative, not left");
}

void test_errors_and_exhaustion() {
    g_cas_pool.reset();
    const char* err = nullptr;
    check(parse_expr("2 +", &err) == nullptr, "trailing operator errors");
    check(parse_expr("(x+1", &err) == nullptr, "unbalanced paren errors");
    check(parse_expr("2 2 x", nullptr) != nullptr, "implicit mult of literals ok");

    // Pool exhaustion returns nullptr rather than crashing.
    g_cas_pool.reset();
    int n = 0;
    while (n < 1000000 && Expr::num(1.0) != nullptr) {
        ++n;
    }
    check(n > 100, "pool holds a reasonable number of nodes");
    check(n < 1000000, "pool eventually reports exhaustion");
    check(Expr::add(Expr::num(1), Expr::num(2)) == nullptr, "alloc fails cleanly when full");
    g_cas_pool.reset();
    check(Expr::num(1) != nullptr, "reset reclaims the pool");
}

// simplify(input) and simplify(expected) produce structurally equal trees.
void check_simplifies(const char* input, const char* expected) {
    g_cas_pool.reset();
    Expr* a = parse_expr(input, nullptr);
    Expr* b = parse_expr(expected, nullptr);
    ++g_checks;
    if (a == nullptr || b == nullptr) {
        ++g_failures;
        std::printf("FAIL: parse null in check_simplifies(\"%s\",\"%s\")\n", input, expected);
        return;
    }
    Expr* sa = simplify(a);
    Expr* sb = simplify(b);
    if (sa == nullptr || sb == nullptr || !sa->equals(sb)) {
        ++g_failures;
        char got[128] = "?";
        char want[128] = "?";
        if (sa != nullptr) {
            math::cas::expr_to_string(sa, got, sizeof(got));
        }
        if (sb != nullptr) {
            math::cas::expr_to_string(sb, want, sizeof(want));
        }
        std::printf("FAIL: simplify(\"%s\") = \"%s\", expected form of \"%s\" = \"%s\"\n", input,
                    got, expected, want);
    }
}

// simplify(input) is exactly the numeric literal `value`.
void check_simplify_num(const char* input, double value) {
    g_cas_pool.reset();
    Expr* a = parse_expr(input, nullptr);
    ++g_checks;
    if (a == nullptr) {
        ++g_failures;
        std::printf("FAIL: parse null in check_simplify_num(\"%s\")\n", input);
        return;
    }
    Expr* s = simplify(a);
    if (s == nullptr || !s->is_num() || s->num_val != value) {
        ++g_failures;
        std::printf("FAIL: simplify(\"%s\") not num %.12g\n", input, value);
    }
}

void test_simplify_identity() {
    check_simplifies("x + 0", "x");
    check_simplifies("0 + x", "x");
    check_simplifies("x*1", "x");
    check_simplifies("1*x", "x");
    check_simplify_num("x*0", 0.0);
    check_simplify_num("x^0", 1.0);
    check_simplifies("x^1", "x");
    check_simplifies("-(-x)", "x");
    check_simplify_num("0^5", 0.0);
    check_simplify_num("1^x", 1.0);
}

void test_simplify_constfold() {
    check_simplify_num("2 + 3", 5.0);
    check_simplify_num("4*5", 20.0);
    check_simplify_num("2^10", 1024.0);
    check_simplify_num("2 - 2", 0.0);
    check_simplify_num("10/2", 5.0);
    check_simplify_num("sin(0)", 0.0);
    check_simplify_num("cos(0)", 1.0);
    check_simplify_num("ln(1)", 0.0);
    check_simplify_num("sqrt(0)", 0.0);
    check_simplify_num("sqrt(1)", 1.0);
    check_simplify_num("abs(-3)", 3.0);
}

void test_simplify_like_terms() {
    check_simplifies("3x + 5x", "8x");
    check_simplifies("x + x", "2x");
    check_simplifies("2x + 3y + x", "3x + 3y");
    check_simplify_num("x - x", 0.0);
    check_simplifies("y + x + 1", "x + y + 1");
}

void test_simplify_like_factors() {
    check_simplifies("x^2 * x^3", "x^5");
    check_simplifies("x*x", "x^2");
    check_simplifies("x*x*x", "x^3");
    check_simplifies("2*x*3", "6x");
}

void test_simplify_imaginary() {
    check_simplify_num("i*i", -1.0);
    check_simplify_num("i^2", -1.0);
    check_simplifies("i^3", "-i");
    check_simplify_num("i^4", 1.0);
    check_simplify_num("2*i*i", -2.0);
    check_simplify_num("i*i*i*i", 1.0);
}

void test_simplify_fractions() {
    check_simplify_num("x/x", 1.0);
    check_simplifies("a/a", "1");
    check_simplifies("2x/(4x^2)", "1/(2x)");
    check_simplifies("6x^3/(2x)", "3x^2");
}

void test_simplify_commutativity() {
    check_simplifies("x + y", "y + x");
    check_simplifies("x*y", "y*x");
    check_simplifies("3x + 5x", "5x + 3x");
}

void test_simplify_termination() {
    // Risk-1 tricky expressions must terminate (no infinite rule cycling).
    g_cas_pool.reset();
    check(simplify(parse_expr("0^0", nullptr)) != nullptr, "0^0 terminates");
    g_cas_pool.reset();
    check(simplify(parse_expr("x/x", nullptr)) != nullptr, "x/x terminates");
    g_cas_pool.reset();
    Expr* big = simplify(parse_expr("(x+y)^20", nullptr));
    check(big != nullptr && big->is_pow(), "(x+y)^20 stays a POW (not expanded)");
    g_cas_pool.reset();
    check(simplify(parse_expr("i^4", nullptr)) != nullptr, "i^4 terminates");
}

// expand(input) equals the (simplified) expected form structurally.
void check_expand(const char* input, const char* expected) {
    g_cas_pool.reset();
    Expr* in = parse_expr(input, nullptr);
    Expr* ex = parse_expr(expected, nullptr);
    ++g_checks;
    if (in == nullptr || ex == nullptr) {
        ++g_failures;
        std::printf("FAIL: parse null in check_expand(\"%s\")\n", input);
        return;
    }
    Expr* xd = expand(in);
    Expr* exs = simplify(ex);
    if (xd == nullptr || exs == nullptr || !xd->equals(exs)) {
        ++g_failures;
        char got[160] = "?";
        if (xd != nullptr) {
            math::cas::expr_to_string(xd, got, sizeof(got));
        }
        std::printf("FAIL: expand(\"%s\") = \"%s\", expected \"%s\"\n", input, got, expected);
    }
}

// expand(input) is value-equivalent to the input at sample points.
void check_expand_value(const char* input) {
    g_cas_pool.reset();
    Expr* in = parse_expr(input, nullptr);
    ++g_checks;
    if (in == nullptr) {
        ++g_failures;
        return;
    }
    Expr* xd = expand(in);
    const double xs[] = {0.3, 0.7, 1.3, 2.1};
    for (double x : xs) {
        if (xd == nullptr || std::fabs(eval(xd, 'x', x) - eval(in, 'x', x)) > 1e-6) {
            ++g_failures;
            std::printf("FAIL: expand(\"%s\") not value-equal at %g\n", input, x);
            return;
        }
    }
}

void test_expand() {
    check_expand("(x+1)*(x-1)", "x^2 - 1");
    check_expand("(x+1)^2", "x^2 + 2x + 1");
    check_expand("(x+1)^3", "x^3 + 3x^2 + 3x + 1");
    check_expand("2*(x+3)", "2x + 6");
    check_expand("(x+2)*(x+3)", "x^2 + 5x + 6");
    check_expand("x*(x+1)", "x^2 + x");
    // Value-preserving on larger / mixed cases.
    check_expand_value("(x+1)^10");
    check_expand_value("(2x-3)^4");
    check_expand_value("(x+1)*(x+2)*(x+3)");
}

// solve(eq) returns exactly the expected solution set (order-independent).
void check_solve_set(const char* eq, char var, bool allow_complex, const char* const* expected,
                     int n) {
    g_cas_pool.reset();
    Expr* e = parse_expr(eq, nullptr);
    ++g_checks;
    if (e == nullptr) {
        ++g_failures;
        std::printf("FAIL: parse null in solve(\"%s\")\n", eq);
        return;
    }
    SolveResult r = solve(e, var, allow_complex);
    if (r.count != n) {
        ++g_failures;
        std::printf("FAIL: solve(\"%s\") count %d != %d\n", eq, r.count, n);
        return;
    }
    bool used[8] = {};
    for (int i = 0; i < n; ++i) {
        Expr* want = simplify(parse_expr(expected[i], nullptr));
        bool found = false;
        for (int j = 0; j < r.count; ++j) {
            if (!used[j] && r.solutions[j] != nullptr && r.solutions[j]->equals(want)) {
                used[j] = true;
                found = true;
                break;
            }
        }
        if (!found) {
            ++g_failures;
            std::printf("FAIL: solve(\"%s\") missing root \"%s\"\n", eq, expected[i]);
            return;
        }
    }
}

// solve(eq) has `count` roots, each satisfying f(root) ~ 0 numerically.
void check_solve_numeric(const char* eq, char var, int count) {
    g_cas_pool.reset();
    Expr* e = parse_expr(eq, nullptr);
    ++g_checks;
    if (e == nullptr || !e->is_eq()) {
        ++g_failures;
        std::printf("FAIL: parse null/non-eq in solve(\"%s\")\n", eq);
        return;
    }
    Expr* f = Expr::add(e->child->clone(), Expr::neg(e->child->next->clone()));
    SolveResult r = solve(e, var, true);
    if (r.count != count) {
        ++g_failures;
        std::printf("FAIL: solve(\"%s\") count %d != %d\n", eq, r.count, count);
        return;
    }
    for (int j = 0; j < r.count; ++j) {
        const double xv = eval(r.solutions[j], var, 0.0);
        if (std::fabs(eval(f, var, xv)) > 1e-6) {
            ++g_failures;
            std::printf("FAIL: solve(\"%s\") root %d does not satisfy eqn\n", eq, j);
            return;
        }
    }
}

// factor(input) equals expected structurally and is value-equal to input.
void check_factor(const char* input, const char* expected) {
    g_cas_pool.reset();
    Expr* in = parse_expr(input, nullptr);
    Expr* exp = parse_expr(expected, nullptr);
    ++g_checks;
    if (in == nullptr || exp == nullptr) {
        ++g_failures;
        std::printf("FAIL: parse null in check_factor(\"%s\")\n", input);
        return;
    }
    Expr* fac = factor(in, 'x');
    Expr* exps = simplify(exp);
    if (fac == nullptr || exps == nullptr || !fac->equals(exps)) {
        ++g_failures;
        char got[160] = "?";
        if (fac != nullptr) {
            math::cas::expr_to_string(fac, got, sizeof(got));
        }
        std::printf("FAIL: factor(\"%s\") = \"%s\", expected \"%s\"\n", input, got, expected);
        return;
    }
    const double xs[] = {0.3, 1.3, 2.5};
    for (double x : xs) {
        if (std::fabs(eval(fac, 'x', x) - eval(in, 'x', x)) > 1e-6) {
            ++g_failures;
            std::printf("FAIL: factor(\"%s\") not value-equal at %g\n", input, x);
            return;
        }
    }
}

void test_factor() {
    check_factor("x^2 - 4", "(x-2)*(x+2)");
    check_factor("x^2 - 1", "(x-1)*(x+1)");
    check_factor("x^3 - 6x^2 + 11x - 6", "(x-1)*(x-2)*(x-3)");
    check_factor("6x^3 + 4x^2", "2*x^2*(3x+2)");
    check_factor("2x + 4", "2*(x+2)");
    check_factor("x^4 - 5x^2 + 4", "(x-1)*(x+1)*(x-2)*(x+2)");
    check_factor("x^2 + 1", "x^2 + 1");  // irreducible over the reals
}

// ∫f is validated by the fundamental theorem: d/dx(∫f) == f at sample points.
void check_integrate(const char* input, char var) {
    g_cas_pool.reset();
    Expr* f = parse_expr(input, nullptr);
    ++g_checks;
    if (f == nullptr) {
        ++g_failures;
        std::printf("FAIL: parse null in integrate(\"%s\")\n", input);
        return;
    }
    Expr* big_f = integrate(f, var);
    if (big_f == nullptr) {
        ++g_failures;
        std::printf("FAIL: integrate(\"%s\") -> null\n", input);
        return;
    }
    Expr* d = differentiate(big_f, var);
    if (d == nullptr) {
        ++g_failures;
        std::printf("FAIL: d/d%c of integral of \"%s\" -> null\n", var, input);
        return;
    }
    const double xs[] = {0.4, 0.9, 1.6, 2.3};
    for (double x : xs) {
        if (std::fabs(eval(d, var, x) - eval(f, var, x)) > 1e-5) {
            ++g_failures;
            std::printf("FAIL: integrate(\"%s\") fails d/dx check at %g\n", input, x);
            return;
        }
    }
}

void test_integrate() {
    check_integrate("x^3", 'x');
    check_integrate("x^2 + 3x + 2", 'x');
    check_integrate("1/x", 'x');
    check_integrate("sin(x)", 'x');
    check_integrate("cos(x)", 'x');
    check_integrate("exp(x)", 'x');
    check_integrate("sin(3x + 1)", 'x');  // linear substitution
    check_integrate("exp(2x)", 'x');
    check_integrate("x*exp(x)", 'x');  // integration by parts
    check_integrate("ln(x)", 'x');     // by-parts closed form
    check_integrate("5", 'x');         // constant

    // Definite: symbolic antiderivative path.
    g_cas_pool.reset();
    auto r = math::cas::definite_integrate(parse_expr("sin(x)", nullptr), 'x',
                                           parse_expr("0", nullptr), parse_expr("pi", nullptr));
    ++g_checks;
    if (!r.has_numeric || std::fabs(r.numeric_val - 2.0) > 1e-6) {
        ++g_failures;
        std::printf("FAIL: definite integral of sin 0..pi != 2 (got %.10g)\n", r.numeric_val);
    }
    // Definite: numeric fallback (no elementary antiderivative).
    g_cas_pool.reset();
    auto r2 = math::cas::definite_integrate(parse_expr("exp(x^2)", nullptr), 'x',
                                            parse_expr("0", nullptr), parse_expr("1", nullptr));
    ++g_checks;
    if (!r2.has_numeric || std::fabs(r2.numeric_val - 1.46265) > 1e-3) {
        ++g_failures;
        std::printf("FAIL: numeric fallback integral exp(x^2) 0..1 (got %.10g)\n", r2.numeric_val);
    }
}

void test_solve() {
    const char* lin[] = {"4"};
    check_solve_set("3x + 5 = 17", 'x', true, lin, 1);
    const char* q1[] = {"2", "3"};
    check_solve_set("x^2 - 5x + 6 = 0", 'x', true, q1, 2);
    const char* q2[] = {"2", "-2"};
    check_solve_set("x^2 - 4 = 0", 'x', true, q2, 2);
    // Complex roots (allowed).
    const char* c1[] = {"i", "-i"};
    check_solve_set("x^2 + 1 = 0", 'x', true, c1, 2);
    const char* c2[] = {"1 + 2i", "1 - 2i"};
    check_solve_set("x^2 - 2x + 5 = 0", 'x', true, c2, 2);
    // REAL mode: complex roots suppressed.
    g_cas_pool.reset();
    SolveResult r = solve(parse_expr("x^2 + 1 = 0", nullptr), 'x', false);
    ++g_checks;
    if (r.count != 0 || !r.complex) {
        ++g_failures;
        std::printf("FAIL: REAL-mode x^2+1=0 should have no real solution\n");
    }
    // Irrational roots checked by value.
    check_solve_numeric("x^2 - 2 = 0", 'x', 2);
    check_solve_numeric("x^2 - 3x + 1 = 0", 'x', 2);
    // Inverse-function isolation.
    g_cas_pool.reset();
    SolveResult s = solve(parse_expr("sin(x) = 1/2", nullptr), 'x', true);
    ++g_checks;
    if (s.count != 1 || std::fabs(eval(s.solutions[0], 'x', 0.0) - M_PI / 6.0) > 1e-6) {
        ++g_failures;
        std::printf("FAIL: solve(sin(x)=1/2) != pi/6\n");
    }
    const char* e1[] = {"0"};
    check_solve_set("exp(x) = 1", 'x', true, e1, 1);
}

void test_derivative() {
    check_deriv("x", 'x', "1");
    check_deriv("x^3", 'x', "3*x^2");
    check_deriv("x^2 + 3x + 1", 'x', "2x + 3");
    check_deriv("sin(x)", 'x', "cos(x)");
    check_deriv("cos(x)", 'x', "-sin(x)");
    check_deriv("tan(x)", 'x', "1 + tan(x)^2");
    check_deriv("exp(x)", 'x', "exp(x)");
    check_deriv("ln(x)", 'x', "1/x");
    check_deriv("sqrt(x)", 'x', "1/(2*sqrt(x))");
    // Product rule + chain rule.
    check_deriv("x^3*sin(x)", 'x', "3*x^2*sin(x) + x^3*cos(x)");
    check_deriv("sin(x^2)", 'x', "2*x*cos(x^2)");
    check_deriv("exp(2x)", 'x', "2*exp(2x)");
    // Quotient (via negative power) — checked by value, not canonical form.
    check_deriv("x/(x+1)", 'x', "1/(x+1)^2");
    // General power rule u^v.
    check_deriv("x^x", 'x', "x^x*(ln(x) + 1)");
    // atan/asin chains.
    check_deriv("atan(x)", 'x', "1/(1 + x^2)");
    // Higher-order: d^2/dx^2 x^4 = 12x^2.
    g_cas_pool.reset();
    Expr* d2 = math::cas::differentiate_n(parse_expr("x^4", nullptr), 'x', 2);
    ++g_checks;
    if (d2 == nullptr || !simplify(d2)->equals(simplify(parse_expr("12*x^2", nullptr)))) {
        ++g_failures;
        std::printf("FAIL: d^2/dx^2 x^4 != 12x^2\n");
    }
}

// ---- Stage 3a: inline CAS home-screen routing (evaluate_home, 4D.21) ----

using math::cas::evaluate_home;
using math::cas::HomeKind;
using math::cas::HomeResult;

// A recognized CAS call yields a single-expression result matching `expected`
// by value at several sample points.
void check_home_value(const char* input, const char* expected, char var) {
    const HomeResult r = evaluate_home(input, true);
    ++g_checks;
    if (r.kind != HomeKind::kExpr || r.result == nullptr) {
        ++g_failures;
        std::printf("FAIL: evaluate_home(\"%s\") not an expr (kind=%d)\n", input,
                    static_cast<int>(r.kind));
        return;
    }
    // `expected` parses in the same pool region after evaluate_home ran, so
    // clone the result first, then parse the reference — both stay valid.
    Expr* got = r.result;
    Expr* want = parse_expr(expected, nullptr);
    for (double x = 0.3; x <= 2.0; x += 0.7) {
        ++g_checks;
        if (std::fabs(eval(got, var, x) - eval(want, var, x)) > 1e-6) {
            ++g_failures;
            std::printf("FAIL: evaluate_home(\"%s\") != %s at x=%.1f\n", input, expected, x);
            break;
        }
    }
}

void test_home_eval() {
    // Not a CAS call -> kNone (falls through to the numeric paths).
    check(evaluate_home("2+3", true).kind == HomeKind::kNone, "home: 2+3 -> none");
    check(evaluate_home("sin(x)", true).kind == HomeKind::kNone, "home: sin(x) -> none");
    check(evaluate_home("", true).kind == HomeKind::kNone, "home: empty -> none");

    // The six ops route and compute.
    check_home_value("simplify(2x + 3x)", "5x", 'x');
    check_home_value("expand((x+1)^2)", "x^2 + 2x + 1", 'x');
    check_home_value("factor(x^2 - 4)", "x^2 - 4", 'x');          // value-equivalent
    check_home_value("diff(x^3)", "3x^2", 'x');                   // default var x
    check_home_value("diff(x^3*sin(x), x)", "3x^2*sin(x) + x^3*cos(x)", 'x');
    check_home_value("diff(x^4, x, 2)", "12x^2", 'x');            // 2nd derivative
    check_home_value("integ(x^3)", "x^4/4", 'x');
    check_home_value("factor(x^2 - 5x + 6)", "(x-2)*(x-3)", 'x');

    // factor really factors (structure, not just value).
    {
        const HomeResult r = evaluate_home("factor(x^2 - 4)", true);
        char buf[64];
        math::cas::expr_to_string(r.result, buf, sizeof(buf));
        check(std::strstr(buf, "x - 2") != nullptr && std::strstr(buf, "x + 2") != nullptr,
              "home: factor(x^2-4) -> (x-2)(x+2)");
    }

    // Rational coefficients serialize as fractions, not decimals (so the
    // layout builder typesets a real stacked fraction) — integ(x^3) = x^4/4.
    {
        const HomeResult r = evaluate_home("integ(x^3)", true);
        char buf[64];
        math::cas::expr_to_string(r.result, buf, sizeof(buf));
        check(std::strcmp(buf, "x^4 / 4") == 0, "home: integ(x^3) serializes x^4 / 4");
        check(std::strchr(buf, '.') == nullptr, "home: integ(x^3) has no decimal point");
    }
    {
        const HomeResult r = evaluate_home("integ(x^2)", true);
        char buf[64];
        math::cas::expr_to_string(r.result, buf, sizeof(buf));
        check(std::strcmp(buf, "x^3 / 3") == 0, "home: integ(x^2) serializes x^3 / 3");
    }

    // Definite integral -> numeric value.
    {
        const HomeResult r = evaluate_home("integ(sin(x), x, 0, pi)", true);
        check(r.kind == HomeKind::kExpr && r.result != nullptr &&
                  std::fabs(eval(r.result, 'x', 0.0) - 2.0) < 1e-4,
              "home: integ(sin x,0,pi) = 2");
    }

    // solve: real roots.
    {
        const HomeResult r = evaluate_home("solve(x^2 - 5x + 6 = 0, x)", true);
        check(r.kind == HomeKind::kSolutions && r.count == 2, "home: solve x^2-5x+6 -> 2 roots");
    }
    // solve: complex roots gated by allow_complex.
    {
        const HomeResult rc = evaluate_home("solve(x^2 + 1 = 0, x)", true);
        check(rc.kind == HomeKind::kSolutions && rc.count == 2 && rc.complex,
              "home: solve x^2+1 (complex) -> 2 roots");
        const HomeResult rr = evaluate_home("solve(x^2 + 1 = 0, x)", false);
        check(rr.kind == HomeKind::kError, "home: solve x^2+1 (real) -> error");
    }
    // Shape split (P5-4): a numeric guess/bounds is the numeric solver's job.
    check(evaluate_home("solve(x^2 - 2, x, 0, 2)", true).kind == HomeKind::kNone,
          "home: solve with bounds -> none (numeric)");
    check(evaluate_home("solve(cos(x) - x, x, 0.5)", true).kind == HomeKind::kNone,
          "home: solve with guess -> none (numeric)");

    // Malformed calls report an error, they don't fall through or crash.
    check(evaluate_home("diff()", true).kind == HomeKind::kError, "home: diff() -> error");
}

// ---- Exact-form display (Stage 4, 4D.23/4D.24) -----------------------------

// The numeric value the home screen would have shown, from the same engine
// the real dispatch uses — so the G5 agreement gate is exercised against a
// genuinely independent number, not against exact_form's own arithmetic.
void check_exact(const char* input, const char* want) {
    const auto num = math::engine().evaluate(input);
    char out[48];
    const bool got = math::cas::exact_form(input, num.ok ? num.value : 0.0, out, sizeof(out));
    ++g_checks;
    if (want == nullptr) {
        if (got) {
            ++g_failures;
            std::printf("FAIL: exact(\"%s\") -> \"%s\", expected no upgrade\n", input, out);
        }
        return;
    }
    if (!got) {
        ++g_failures;
        std::printf("FAIL: exact(\"%s\") -> none, expected \"%s\"\n", input, want);
        return;
    }
    if (std::strcmp(out, want) != 0) {
        ++g_failures;
        std::printf("FAIL: exact(\"%s\") -> \"%s\", expected \"%s\"\n", input, out, want);
    }
}

void test_exact_form() {
    // Surd extraction: perfect-square factors are pulled out of the radicand.
    check_exact("sqrt(2)", "sqrt(2)");
    check_exact("sqrt(8)", "2*sqrt(2)");
    check_exact("sqrt(12)", "2*sqrt(3)");
    check_exact("sqrt(72)", "6*sqrt(2)");
    // Rationalization: a radical in a denominator moves to the numerator.
    check_exact("1/sqrt(2)", "sqrt(2) / 2");
    check_exact("sqrt(1/2)", "sqrt(2) / 2");
    check_exact("sqrt(18)/3", "sqrt(2)");
    // Like radicals combine (this is the simplifier doing the work for us).
    check_exact("sqrt(2)+sqrt(8)", "3*sqrt(2)");
    // pi keeps its closed form too (P5-6).
    check_exact("pi", "pi");
    check_exact("pi/2", "pi / 2");
    check_exact("pi*2", "2*pi");
    check_exact("3*pi/4", "3*pi / 4");
    check_exact("1/pi", "1 / pi");
    // Bare rationals: an integers-only input means an exact answer was meant.
    check_exact("1/3", "1/3");
    check_exact("2/6", "1/3");
    check_exact("1/3+1/7", "10/21");
    // A sum of a radical and a rational.
    check_exact("1+sqrt(2)", "sqrt(2) + 1");

    // G2 — a non-integer literal means the user typed a decimal, so leave the
    // decimal alone. Without this, 2.5 would become 5/2 and 0.1+0.2 -> 3/10.
    check_exact("2.5", nullptr);
    check_exact("0.1+0.2", nullptr);
    check_exact("1e-3", nullptr);
    // G3 — anything naming a symbol. The CAS parser has no `ans` or `e`, so
    // these must never be evaluated symbolically.
    check_exact("e", nullptr);
    check_exact("ans", nullptr);
    check_exact("x+1", nullptr);
    check_exact("2->a", nullptr);
    // G4 — "interesting": a bare integer result is already exact.
    check_exact("2+2", nullptr);
    check_exact("4/2", nullptr);
    check_exact("sqrt(4)", nullptr);
    // G4 — outside the whitelist grammar.
    check_exact("sin(1)", nullptr);
    check_exact("ln(2)", nullptr);
    check_exact("sqrt(pi)", nullptr);
    check_exact("sqrt(sqrt(2))", nullptr);
    check_exact("pi^2", nullptr);
    check_exact("2^(1/3)", nullptr);
    check_exact("sqrt(-4)", nullptr);
    // Bounds: a radicand past the factorizer's cap, and a rational the
    // serializer would print as a decimal anyway.
    check_exact("sqrt(1000000007)", nullptr);
    check_exact("1/10001", nullptr);

    // G5 is wired and not vacuous: an exact form that disagrees with the
    // numeric result is rejected, whatever the shape says.
    {
        char out[48];
        check(!math::cas::exact_form("sqrt(2)", 99.0, out, sizeof(out)),
              "exact: G5 rejects a form that disagrees with the numeric value");
    }

    // Pool exhaustion returns false rather than crashing (spec §13 Risk 2).
    {
        char deep[64];
        std::memset(deep, '(', sizeof(deep));
        deep[sizeof(deep) - 1] = 0;
        char out[48];
        check(!math::cas::exact_form(deep, 1.0, out, sizeof(out)),
              "exact: malformed/deep input returns false");
    }

    // The probe leaves `out` untouched when it declines, so a caller that
    // ignores the return value cannot show garbage.
    {
        char out[48] = "sentinel";
        (void)math::cas::exact_form("2+2", 4.0, out, sizeof(out));
        check(std::strcmp(out, "sentinel") == 0, "exact: declined probe leaves out untouched");
    }
}

// ---- Exact trigonometric values at special angles --------------------------

void test_exact_trig() {
    const math::AngleMode saved = math::angle_mode();

    math::set_angle_mode(math::AngleMode::kRadians);
    check_exact("sin(pi/6)", "1/2");
    check_exact("sin(pi/4)", "sqrt(2) / 2");
    check_exact("sin(pi/3)", "sqrt(3) / 2");
    check_exact("cos(pi/6)", "sqrt(3) / 2");
    check_exact("cos(pi/3)", "1/2");
    check_exact("cos(-pi/3)", "1/2");
    check_exact("tan(pi/6)", "sqrt(3) / 3");
    check_exact("tan(pi/3)", "sqrt(3)");
    check_exact("sin(-pi/6)", "-1/2");
    // Float noise the numeric path can't avoid: sin(pi) lands on 1.22e-16
    // and cos(pi/2) on 6.12e-17, which display as scientific notation.
    check_exact("sin(pi)", "0");
    check_exact("cos(pi/2)", "0");
    // Exact values the numeric path already displays correctly are not
    // upgraded — sin(pi/2) is 1 either way, and tan(pi/4)'s
    // 0.9999999999999999 already formats as "1".
    check_exact("sin(pi/2)", nullptr);
    check_exact("tan(pi/4)", nullptr);
    check_exact("cos(pi)", nullptr);
    // tan is undefined at odd multiples of pi/2 — left alone, never folded.
    check_exact("tan(pi/2)", nullptr);
    // Angles outside the pi/6 and pi/4 families have no exact value.
    check_exact("sin(pi/5)", nullptr);
    check_exact("tan(pi/7)", nullptr);
    check_exact("sin(1)", nullptr);
    check_exact("sin(2)", nullptr);
    // Folded trig composes with the rest of the grammar.
    check_exact("2*sin(pi/3)", "sqrt(3)");
    check_exact("sin(pi/3)+sin(pi/6)", "sqrt(3) / 2 + 1/2");

    // DEGREE mode reads the argument as degrees, so the same special angles
    // are reachable without typing pi.
    math::set_angle_mode(math::AngleMode::kDegrees);
    check_exact("sin(30)", "1/2");
    check_exact("sin(45)", "sqrt(2) / 2");
    check_exact("sin(60)", "sqrt(3) / 2");
    check_exact("cos(30)", "sqrt(3) / 2");
    check_exact("tan(60)", "sqrt(3)");
    check_exact("sin(-60)", "-sqrt(3) / 2");
    check_exact("sin(120)", "sqrt(3) / 2");
    check_exact("sin(180)", "0");
    check_exact("sin(90)", nullptr);
    check_exact("tan(45)", nullptr);
    // Non-special degree angles, and a radian-style argument typed while in
    // DEGREE mode (G5 would reject it even if the fold were attempted).
    check_exact("sin(37)", nullptr);
    check_exact("sin(10)", nullptr);
    check_exact("sin(pi/3)", nullptr);

    math::set_angle_mode(saved);
}

// ---- Stage 5 stress + edge cases (task 4D.22, decisions D45) --------------

// Build the nesting ladder the 2026-08-05 hardware session walked:
// rung 1 = "(2+1)^2+1", rung n wraps the previous in "(...)^2+1". Each rung is
// one more level of ADD-inside-POW-inside-ADD, which is one more nested
// simplify_sum. Before D45 those were ~1.1 KB stack frames each and rung 6
// overran core 0's 4 KB stack into core 1's, silently returning the right
// answer (1.173e32) while scribbling on memory nothing happened to be using.
void build_rung(int n, char* out, std::size_t cap) {
    std::snprintf(out, cap, "2+1");
    for (int i = 0; i < n; ++i) {
        char wrapped[256];
        std::snprintf(wrapped, sizeof(wrapped), "(%s)^2+1", out);
        std::snprintf(out, cap, "%s", wrapped);
    }
}

// Expected value of rung n: v0 = 3, v(k+1) = v(k)^2 + 1.
double rung_value(int n) {
    double v = 3.0;
    for (int i = 0; i < n; ++i) {
        v = v * v + 1.0;
    }
    return v;
}

void test_stress_nesting() {
    // The ladder must produce the right number at every rung, and must fail
    // cleanly (never a wrong number, never a crash) once past the depth cap.
    for (int n = 1; n <= 10; ++n) {
        char expr[256];
        build_rung(n, expr, sizeof(expr));
        g_cas_pool.reset();
        Expr* t = parse_expr(expr, nullptr);
        if (t == nullptr) {
            continue;  // past the parser's nesting cap: a clean refusal
        }
        Expr* s = simplify(t);
        ++g_checks;
        if (s == nullptr || g_cas_pool.overflowed()) {
            continue;  // clean abort past the simplifier's depth cap
        }
        if (!s->is_num() || std::fabs(s->num_val - rung_value(n)) > 1e-6 * rung_value(n)) {
            ++g_failures;
            char got[128] = "?";
            expr_to_string(s, got, sizeof(got));
            std::printf("FAIL: rung %d simplify -> %s, expected %g\n", n, got, rung_value(n));
        }
    }

    // Rung 5 is the deepest that fits kMaxInputLen (48) on the home screen,
    // and it is inside the depth cap, so it must still give a real answer.
    {
        char expr[256];
        build_rung(5, expr, sizeof(expr));
        g_cas_pool.reset();
        Expr* s = simplify(parse_expr(expr, nullptr));
        check(s != nullptr && s->is_num() &&
                  std::fabs(s->num_val - rung_value(5)) < 1e-6 * rung_value(5),
              "rung 5 still computes");
    }
}

void test_stress_parser_depth() {
    // Nested parens well past the cap: refused, not a stack walk.
    {
        char buf[256];
        std::size_t i = 0;
        for (; i < 40; ++i) {
            buf[i] = '(';
        }
        buf[i++] = 'x';
        for (int k = 0; k < 40; ++k) {
            buf[i++] = ')';
        }
        buf[i] = '\0';
        g_cas_pool.reset();
        const char* err = nullptr;
        check(parse_expr(buf, &err) == nullptr, "40 nested parens refused");
        check(err != nullptr && std::strcmp(err, "too deeply nested") == 0,
              "40 nested parens report the depth error");
    }

    // A long unary-sign chain recurses without passing through
    // parse_equation, so it needs its own count against the same cap.
    {
        char buf[64];
        std::size_t i = 0;
        for (; i < 40; ++i) {
            buf[i] = '-';
        }
        buf[i++] = 'x';
        buf[i] = '\0';
        g_cas_pool.reset();
        const char* err = nullptr;
        check(parse_expr(buf, &err) == nullptr, "40-long sign chain refused");
        check(err != nullptr && std::strcmp(err, "too deeply nested") == 0,
              "sign chain reports the depth error");
    }

    // Nested function arguments take the same path as parens.
    {
        char buf[256];
        buf[0] = '\0';
        std::size_t used = 0;
        for (int k = 0; k < 30; ++k) {
            used += static_cast<std::size_t>(std::snprintf(buf + used, sizeof(buf) - used, "sin("));
        }
        used += static_cast<std::size_t>(std::snprintf(buf + used, sizeof(buf) - used, "x"));
        for (int k = 0; k < 30; ++k) {
            used += static_cast<std::size_t>(std::snprintf(buf + used, sizeof(buf) - used, ")"));
        }
        g_cas_pool.reset();
        check(parse_expr(buf, nullptr) == nullptr, "30 nested sin() refused");
    }

    // Just inside the cap still parses — the guard must not be off-by-one
    // into rejecting ordinary input.
    {
        g_cas_pool.reset();
        check(parse_expr("((((((x+1))))))", nullptr) != nullptr, "6 nested parens still parse");
        g_cas_pool.reset();
        check(parse_expr("sin(cos(tan(x)))", nullptr) != nullptr, "3 nested funcs still parse");
    }
}

// simplify() must be idempotent: a second pass over a canonical tree changes
// nothing. This is the real Risk-1 termination property — a rule cycle shows
// up as simplify(simplify(e)) != simplify(e) — and it needs no new API.
void check_idempotent(const char* input) {
    g_cas_pool.reset();
    Expr* a = parse_expr(input, nullptr);
    ++g_checks;
    if (a == nullptr) {
        ++g_failures;
        std::printf("FAIL: idempotence parse null for \"%s\"\n", input);
        return;
    }
    Expr* once = simplify(a);
    if (once == nullptr) {
        return;  // a clean abort is acceptable; a wrong answer is not
    }
    char first[192] = "?";
    expr_to_string(once, first, sizeof(first));
    Expr* twice = simplify(once);
    if (twice == nullptr) {
        ++g_failures;
        std::printf("FAIL: simplify(simplify(\"%s\")) -> null\n", input);
        return;
    }
    char second[192] = "?";
    expr_to_string(twice, second, sizeof(second));
    if (std::strcmp(first, second) != 0) {
        ++g_failures;
        std::printf("FAIL: not idempotent: simplify(\"%s\") = \"%s\", again = \"%s\"\n", input,
                    first, second);
    }
}

void test_stress_termination() {
    // The spec §13 Risk-1 set, plus the shapes most likely to cycle: powers
    // that cancel, reciprocals, i, and mixed rational coefficients.
    const char* const corpus[] = {
        "x/x",         "0^0",        "i^4",         "1^x",          "x^0",
        "0*x",         "sqrt(x)^2",  "(x^2)^0.5",   "x^2/x",        "x/x^2",
        "(x+y)^20",    "1/(1/x)",    "1/(1/(1/x))", "x*y/(y*x)",    "(x+1)/(x+1)",
        "2x/(4x^2)",   "i*i*i*i",    "-(-(-(-x)))", "x - x",        "x + x - 2x",
        "(2/3)x",      "3x/6",       "x^1.5*x^0.5", "sin(x)/sin(x)", "(x*y)^2/(x^2*y^2)",
        "sqrt(2)*sqrt(2)", "1/sqrt(2)", "pi*2/pi",  "(x+y+z)*0",    "abs(abs(x))",
    };
    for (const char* s : corpus) {
        check_idempotent(s);
    }

    // Termination is not just convergence — the tricky ones must still be
    // the right value.
    check_simplify_num("x/x", 1.0);
    check_simplify_num("0^0", 1.0);
    check_simplify_num("i^4", 1.0);
    check_simplify_num("1/(1/(1/2))", 0.5);
}

void test_stress_wide_nary() {
    // Wide sums and products must either fold correctly or degrade
    // gracefully — never crash, never corrupt.
    const int widths[] = {10, 50, 100, 200};
    for (int n : widths) {
        char buf[2048];
        std::size_t used = 0;
        for (int k = 0; k < n; ++k) {
            used += static_cast<std::size_t>(
                std::snprintf(buf + used, sizeof(buf) - used, k == 0 ? "x" : "+x"));
        }
        g_cas_pool.reset();
        Expr* t = parse_expr(buf, nullptr);
        ++g_checks;
        if (t == nullptr) {
            continue;
        }
        Expr* s = simplify(t);
        if (s == nullptr || g_cas_pool.overflowed()) {
            continue;  // graceful abort
        }
        // n*x, or the untouched input if it exceeded kMaxOperands.
        if (std::fabs(eval(s, 'x', 2.0) - 2.0 * n) > 1e-9) {
            ++g_failures;
            std::printf("FAIL: %d-term sum simplified to the wrong value\n", n);
        }
    }
}

void test_stress_pool_guards() {
    // reset() clears both the overflow flag and both ends of the arena.
    g_cas_pool.reset();
    check(!g_cas_pool.overflowed(), "reset clears the overflow flag");
    check(!g_cas_pool.near_capacity(), "an empty pool is not near capacity");
    check(g_cas_pool.used() == 0, "reset empties the pool");

    // Filling the node end trips near_capacity() before it trips overflow.
    g_cas_pool.reset();
    bool saw_near_before_overflow = false;
    while (Expr::num(1.0) != nullptr) {
        if (g_cas_pool.near_capacity() && !g_cas_pool.overflowed()) {
            saw_near_before_overflow = true;
        }
    }
    check(saw_near_before_overflow, "near_capacity() warns before the pool actually fails");
    check(g_cas_pool.overflowed(), "a failed alloc sets the overflow flag");
    g_cas_pool.reset();
    check(!g_cas_pool.overflowed(), "reset clears overflow after a real exhaustion");

    // The scratch end is LIFO: a mark taken now is restored on release, so
    // the simplifier's fixed-point passes reuse the same space instead of
    // accumulating (which is why scratch cannot share the node end).
    g_cas_pool.reset();
    const std::size_t mark = g_cas_pool.scratch_mark();
    check(g_cas_pool.alloc_scratch(1024, 8) != nullptr, "scratch allocates");
    check(g_cas_pool.scratch_mark() > mark, "scratch mark advances");
    g_cas_pool.scratch_release(mark);
    check(g_cas_pool.scratch_mark() == mark, "scratch release rewinds");

    // The two ends must meet cleanly rather than overlap.
    g_cas_pool.reset();
    void* big = g_cas_pool.alloc_scratch(math::scratch::kComputeBytes / 2, 8);
    check(big != nullptr, "half the arena is available as scratch");
    int nodes = 0;
    while (Expr::num(1.0) != nullptr) {
        ++nodes;
    }
    check(g_cas_pool.overflowed(), "nodes growing into held scratch fail cleanly");
    check(static_cast<std::size_t>(nodes) * sizeof(Expr) <= math::scratch::kComputeBytes / 2,
          "nodes never overran the held scratch");
    g_cas_pool.reset();
}

void test_stress_risk2_abort() {
    // (x+y+z)^15 is spec §13 Risk 2's own example. It must come back as an
    // error, not as a plausible-looking partial expansion.
    const HomeResult r = evaluate_home("expand((x+y+z)^15)", true);
    check(r.kind == HomeKind::kError, "expand((x+y+z)^15) is an error, not a wrong answer");
    check(r.error != nullptr && std::strcmp(r.error, "Too complex") == 0,
          "expand((x+y+z)^15) reports Too complex");

    // The expansions that legitimately fit must keep working — the guard
    // must not have cost real capability. (x+1)^10 sits at ~76% of the
    // arena, the closest of these to the ceiling.
    check_home_value("expand((x+1)^10)", "x^10+10x^9+45x^8+120x^7+210x^6+252x^5+210x^4+120x^3+45x^2+10x+1",
                     'x');
    check_home_value("expand((x+1)^8)", "x^8+8x^7+28x^6+56x^5+70x^4+56x^3+28x^2+8x+1", 'x');

    // exact_form() resets the pool itself (D41 pool discipline), so a pool
    // left exhausted by an earlier operation must not poison it.
    g_cas_pool.reset();
    while (Expr::num(1.0) != nullptr) {
    }
    check(g_cas_pool.overflowed(), "pool is exhausted going in");
    char out[48];
    check(math::cas::exact_form("sqrt(2)", 1.4142135623730951, out, sizeof(out)) &&
              std::strcmp(out, "sqrt(2)") == 0,
          "exact_form recovers from a previously exhausted pool");
}

void test_stress_degenerate_input() {
    // Degenerate and malformed home-screen input: every one of these must
    // return a defined result, never crash and never a stale pointer.
    const char* const not_cas[] = {"", " ", "   ", "2+3", "x", "(", ")", "()"};
    for (const char* s : not_cas) {
        const HomeResult r = evaluate_home(s, true);
        check(r.kind == HomeKind::kNone || r.kind == HomeKind::kError,
              "degenerate input returns none or error");
    }

    // Recognized op, unusable arguments.
    check(evaluate_home("diff()", true).kind == HomeKind::kError, "diff() errors");
    check(evaluate_home("simplify()", true).kind == HomeKind::kError, "simplify() errors");
    check(evaluate_home("diff(x,,)", true).kind == HomeKind::kError, "diff(x,,) errors");
    check(evaluate_home("diff(x, 9)", true).kind == HomeKind::kError, "non-var 2nd arg errors");
    {
        // Deep parenthesisation inside a recognized op: whatever it returns,
        // it must be one of the defined kinds.
        const HomeResult r = evaluate_home("factor((((((((((x))))))))))", true);
        check(r.kind == HomeKind::kExpr || r.kind == HomeKind::kError,
              "deeply parenthesised factor returns a defined kind");
    }

    // Over-long input is refused rather than truncated into a different
    // expression.
    {
        char buf[512];
        std::size_t used = static_cast<std::size_t>(std::snprintf(buf, sizeof(buf), "simplify("));
        for (int k = 0; k < 200; ++k) {
            used += static_cast<std::size_t>(std::snprintf(buf + used, sizeof(buf) - used, "x+"));
        }
        std::snprintf(buf + used, sizeof(buf) - used, "x)");
        const HomeResult r = evaluate_home(buf, true);
        check(r.kind == HomeKind::kError, "over-long CAS input errors");
    }

    // An equation where both sides are identical: solve must not claim a
    // finite solution set for something true everywhere.
    {
        const HomeResult r = evaluate_home("solve(x=x)", true);
        check(r.kind == HomeKind::kError || r.kind == HomeKind::kSolutions,
              "solve(x=x) returns a defined result");
    }

    // The numeric solver's shape (>=3 args) must still fall through to it.
    check(evaluate_home("solve(x^2-2, x, 1)", true).kind == HomeKind::kNone,
          "solve with a guess falls through to the numeric solver");
}

void test_stress_edge_cases() {
    test_stress_nesting();
    test_stress_parser_depth();
    test_stress_termination();
    test_stress_wide_nary();
    test_stress_pool_guards();
    test_stress_risk2_abort();
    test_stress_degenerate_input();
}

}  // namespace

int main() {
    test_tree_ops();
    test_parse_structure();
    test_roundtrips();
    test_implicit_mult();
    test_encodings();
    test_errors_and_exhaustion();
    test_simplify_identity();
    test_simplify_constfold();
    test_simplify_like_terms();
    test_simplify_like_factors();
    test_simplify_imaginary();
    test_simplify_fractions();
    test_simplify_commutativity();
    test_simplify_termination();
    test_expand();
    test_factor();
    test_derivative();
    test_integrate();
    test_solve();
    test_home_eval();
    test_exact_form();
    test_exact_trig();
    test_stress_edge_cases();

    std::printf("test_cas: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
