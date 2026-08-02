// Host-side tests for the CAS foundations (Phase 5 Stage 0, tasks 4D.1-4D.3):
// the Expr tree + pool (expr.cpp), the recursive-descent parser (parser.cpp),
// and expr_to_string (serialize.cpp). No numeric evaluation yet — that starts
// with the Stage 1 simplifier.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "math/cas/cas_eval.hpp"
#include "math/cas/derivative.hpp"
#include "math/cas/expand.hpp"
#include "math/cas/expr.hpp"
#include "math/cas/factor.hpp"
#include "math/cas/integrate.hpp"
#include "math/cas/parser.hpp"
#include "math/cas/serialize.hpp"
#include "math/cas/simplify.hpp"
#include "math/cas/solve.hpp"

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

    std::printf("test_cas: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
